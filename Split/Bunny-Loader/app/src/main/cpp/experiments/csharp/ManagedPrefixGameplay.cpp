#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ManagedPrefixTooltipTemplate.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/ManagedPrefixRollSelection.h"
#include "content/common/ContentAssets.h"
#include "content/common/GameRefs.h"
#include "content/common/TypeTables.h"
#include "content/items/ModItems.h"
#include "content/items/ModPrefixes.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <charconv>
#include <mutex>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace bl::csharp {
namespace {

using RollableFn = Il2CppArray* (*)(Il2CppObject*, const MethodInfo*);
using CanRollFn = bool (*)(Il2CppObject*, int32_t, const MethodInfo*);
using StatsFn = bool (*)(Il2CppObject*, int32_t, float*, float*, float*, float*,
                        float*, float*, int32_t*, int32_t*, int32_t*, float*,
                        const MethodInfo*);
using GrantFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using RollFn = bool (*)(Il2CppObject*, Il2CppObject*, int32_t*, const MethodInfo*);
using PrefixChanceFn = bool (*)(Il2CppObject*, int32_t, uint8_t*, const MethodInfo*);
using ReforgeFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using InventoryTooltipFn = void (*)(Il2CppObject*, int32_t*, int32_t*, int32_t*,
                                    float, int32_t*, Il2CppArray*, Il2CppArray*,
                                    Il2CppArray*, int32_t*, void*, int32_t*,
                                    const MethodInfo*);

struct Refs {
    int type = -1;
    int damage = -1;
    int useAnimation = -1;
    int mana = -1;
    int stack = -1;
    int knockBack = -1;
    int ammo = -1;
    int accessory = -1;
    int consumable = -1;
    int ranged = -1;
    int melee = -1;
    int magic = -1;
    int summon = -1;
    int prefix = -1;
    int playerDefense = -1;
    int playerLife = -1;
    int playerLifeMax = -1;
    Il2CppClass* intClass = nullptr;
    FieldInfo* names = nullptr;
    FieldInfo* chances = nullptr;
    FieldInfo* tooltipSuffixes = nullptr;
    const MethodInfo* localizedValueMethod = nullptr;
    int reforgeNumLines = -1;
    int reforgeText = -1;
    int reforgeBad = -1;
    const MethodInfo* rollableMethod = nullptr;
    const MethodInfo* nextDoubleMethod = nullptr;
};

Refs g_refs;
std::atomic<bool> g_hooksReady{false};
std::mutex g_installMutex;
RollableFn g_originalRollable = nullptr;
CanRollFn g_originalCanRoll = nullptr;
StatsFn g_originalStats = nullptr;
GrantFn g_originalGrant = nullptr;
RollFn g_originalRoll = nullptr;
PrefixChanceFn g_originalItemPrefix = nullptr;
std::mutex g_chanceInstallMutex;
std::atomic<bool> g_chanceReady{false};
int g_chanceItemTypeOffset = -1;
int g_chancePrefixOffset = -1;
int g_chanceDamageOffset = -1;
int g_chanceValueOffset = -1;
ReforgeFn g_originalReforge = nullptr;
InventoryTooltipFn g_originalInventoryTooltip = nullptr;
std::atomic<bool> g_inventoryTooltipReady{false};
std::atomic<bool> g_reforgeTooltipReady{false};
std::atomic<int> g_reforgeDiagnostics{0};
std::atomic<uint64_t> g_prefixRollAttempts{0};
std::atomic<uint64_t> g_prefixRestoredRolls{0};
std::atomic<uint64_t> g_prefixChosenRolls{0};
std::atomic<uint64_t> g_prefixPreferredRolls{0};
std::atomic<uint64_t> g_prefixWeightedRolls{0};
std::atomic<uint64_t> g_prefixVanillaFallbacks{0};
std::atomic<uint64_t> g_prefixRejectedRolls{0};
std::atomic<uint64_t> g_prefixRollTotalUs{0};
std::atomic<uint64_t> g_prefixRollMaxUs{0};

struct PrefixRollDuration {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ~PrefixRollDuration() {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        const uint64_t us = elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0;
        g_prefixRollTotalUs.fetch_add(us, std::memory_order_relaxed);
        uint64_t previous = g_prefixRollMaxUs.load(std::memory_order_relaxed);
        while (previous < us && !g_prefixRollMaxUs.compare_exchange_weak(
            previous, us, std::memory_order_relaxed)) {}
    }
};

bool nativeRollFallback(Il2CppObject* item, Il2CppObject* random,
                        int32_t* rolled, const MethodInfo* method) {
    g_prefixVanillaFallbacks.fetch_add(1, std::memory_order_relaxed);
    return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;
}

struct RestoreRoll {
    Il2CppObject* item = nullptr;
    int prefix = 0;
    bool used = false;
};
thread_local RestoreRoll g_restoreRoll{};
thread_local Il2CppObject* g_failedRollableItem = nullptr;

struct AppliedItemPrefixState {
    Il2CppObject* item = nullptr;
    int type = 0;
    int prefix = 0;
    int damage = 0;
    int value = 0;
    int damageBonus = 0;
    int valueBonus = 0;
};

thread_local std::array<AppliedItemPrefixState, 16> g_appliedItemPrefixes{};
thread_local size_t g_appliedItemCursor = 0;

AppliedItemPrefixState* rememberedPrefix(Il2CppObject* item) {
    for (auto& previous : g_appliedItemPrefixes)
        if (previous.item == item) return &previous;
    return nullptr;
}

void rememberPrefix(Il2CppObject* item, int type, int prefix,
                    int damage, int value, const ManagedItemPrefixApply& bonus) {
    AppliedItemPrefixState* previous = rememberedPrefix(item);
    if (!previous) {
        previous = &g_appliedItemPrefixes[g_appliedItemCursor];
        g_appliedItemCursor = (g_appliedItemCursor + 1) % g_appliedItemPrefixes.size();
    }
    *previous = {item, type, prefix, damage, value, bonus.damageBonus, bonus.valueBonus};
}

Il2CppArray* staticArray(FieldInfo* field) {
    auto& api = bl::il2cpp::api();
    if (!field || !api.field_get_parent || !api.runtime_class_init) return nullptr;
    // field_static_get_value allocates static storage, but does not run .cctor.
    // Prefix(int > 0) and Prefix(-2) need not initialize PrefixID.Sets before
    // our hooks read ReducedNaturalChance. A null table disabled every managed
    // candidate and let the game's failed explicit prefix fall back to a roll.
    Il2CppClass* owner = api.field_get_parent(field);
    if (!owner) return nullptr;
    api.runtime_class_init(owner);
    Il2CppArray* value = nullptr;
    api.field_static_get_value(field, &value);
    return value;
}

const ManagedPrefixEntry* byType(const std::vector<ManagedPrefixEntry>& list, int type) {
    auto iter = std::find_if(list.begin(), list.end(), [type](const auto& p) {
        return p.type == type;
    });
    return iter == list.end() ? nullptr : &*iter;
}

bool weaponCandidate(Il2CppObject* item, bool vanillaHasRollable) {
    const auto& r = g_refs;
    if (!item || r.type < 0 || r.damage < 0 ||
        r.ammo < 0 || r.consumable < 0) return false;
    const int type = bl::runtime::field<int32_t>(item, r.type);
    if (bl::runtime::field<int32_t>(item, r.damage) <= 0 ||
        bl::runtime::field<int32_t>(item, r.ammo) != 0 ||
        bl::runtime::field<uint8_t>(item, r.consumable)) return false;
    return vanillaHasRollable || bl::runtime::isModItem(type);
}

struct PrefixRpcScope {
    PrefixRpcScope* previous = nullptr;
    std::chrono::steady_clock::time_point end;
    std::array<int,256> can{}, weight{}, efficacy{};
    int requests = 0;
    static thread_local PrefixRpcScope* current;
    PrefixRpcScope() {
        previous=current;
        if (!previous) {
            can.fill(-1);
            weight.fill(-1);
            efficacy.fill(-1);
            end=std::chrono::steady_clock::now()+std::chrono::microseconds(2500);
            current=this;
        }
    }
    ~PrefixRpcScope() { if (!previous) current=nullptr; }
};
thread_local PrefixRpcScope* PrefixRpcScope::current=nullptr;

std::string prefixItemSnapshot(Il2CppObject* item) {
    if (!item || g_refs.type<0 || g_refs.damage<0 || g_refs.mana<0 ||
        g_refs.stack<0 || g_refs.prefix<0) return {};
    const int flags=(g_refs.accessory>=0 &&
                     bl::runtime::field<uint8_t>(item,g_refs.accessory)?1:0) |
                    (g_refs.consumable>=0 &&
                     bl::runtime::field<uint8_t>(item,g_refs.consumable)?2:0) |
                    (g_refs.ranged>=0 &&
                     bl::runtime::field<uint8_t>(item,g_refs.ranged)?4:0);
    return std::to_string(bl::runtime::field<int32_t>(item,g_refs.type))+","+
        std::to_string(bl::runtime::field<int32_t>(item,g_refs.damage))+","+
        std::to_string(bl::runtime::field<int32_t>(item,g_refs.mana))+","+
        std::to_string(bl::runtime::field<int32_t>(item,g_refs.stack))+","+
        std::to_string(bl::runtime::field<uint8_t>(item,g_refs.prefix))+","+
        std::to_string(flags);
}

int queryPrefixWorker(Il2CppObject* item,const ManagedPrefixEntry& p,int kind) {
    auto* scope=PrefixRpcScope::current;
    if (!scope || !item || p.type<1 || p.type>255 ||
        !(p.callbackMask & (kind==0?1:kind==1?2:4))) return -2;
    auto& cache=kind==0?scope->can[static_cast<size_t>(p.type)]:
                kind==1?scope->weight[static_cast<size_t>(p.type)]:
                        scope->efficacy[static_cast<size_t>(p.type)];
    if (cache!=-1) return cache;
    cache=-2;
    if (scope->requests>=4 || g_refs.type<0 || g_refs.damage<0 ||
        g_refs.mana<0 || g_refs.stack<0 || g_refs.prefix<0) return cache;
    const auto bind=managedPrefixCallbackAssemblies();
    const auto assembly=bind->find(p.modId);
    if (assembly==bind->end() ||
        managedPrefixWarmupState(p.modId,assembly->second,p.name)!=1) return cache;
    const auto now=std::chrono::steady_clock::now();
    if (now>=scope->end) return cache;
    const int micros=static_cast<int>(
        std::chrono::duration_cast<std::chrono::microseconds>(scope->end-now).count());
    if (micros<1) return cache;
    const std::string snapshot=prefixItemSnapshot(item);
    if (snapshot.empty()) return cache;
    ++scope->requests;
    std::string answer;
    if (!invokeManagedPrefix(p.modId,assembly->second,p.name,kind,snapshot,
                             &answer,std::min(micros,650))) return cache;
    if ((kind==0 || kind==3) && (answer=="0" || answer=="1"))
        cache=answer=="1"?1:0;
    else if (kind==1 && !answer.empty() && answer.size()<=2 &&
             std::all_of(answer.begin(),answer.end(),[](unsigned char ch) {
                 return ch>='0' && ch<='9';
             })) {
        const int value=answer.size()==1?answer[0]-'0':
            (answer[0]-'0')*10+answer[1]-'0';
        if (value<=20) cache=value;
    }
    return cache;
}

bool queryPrefixApply(Il2CppObject* item,const ManagedPrefixEntry& p,
                      int* damage,int* value) {
    if (!item || !damage || !value || !(p.callbackMask & 8) ||
        g_restoreRoll.item==item) return false;
    const auto assemblies=managedPrefixCallbackAssemblies();
    const auto found=assemblies->find(p.modId);
    if (found==assemblies->end() ||
        managedPrefixWarmupState(p.modId,found->second,p.name)!=1) return false;
    const std::string snapshot=prefixItemSnapshot(item);
    if (snapshot.empty()) return false;
    std::string answer;
    if (!invokeManagedPrefix(p.modId,found->second,p.name,4,snapshot,&answer,650))
        return false;
    const auto comma=answer.find(',');
    if (comma==std::string::npos || answer.find(',',comma+1)!=std::string::npos)
        return false;
    int bonusDamage=0,bonusValue=0;
    const auto parse=[](const char* begin,const char* end,int* out) {
        if (begin==end || end-begin>6) return false;
        const auto result=std::from_chars(begin,end,*out);
        return result.ec==std::errc{} && result.ptr==end;
    };
    if (!parse(answer.data(),answer.data()+comma,&bonusDamage) ||
        !parse(answer.data()+comma+1,answer.data()+answer.size(),&bonusValue) ||
        bonusDamage<0 || bonusDamage>1000 || bonusValue<0 || bonusValue>100000)
        return false;
    *damage=bonusDamage;
    *value=bonusValue;
    return true;
}

std::vector<std::pair<std::string,bool>> queryPrefixTooltip(
    Il2CppObject* item,const ManagedPrefixEntry& p) {
    if (!(p.callbackMask & 16)) return {};
    const auto assemblies=managedPrefixCallbackAssemblies();
    const auto found=assemblies->find(p.modId);
    if (found==assemblies->end() ||
        managedPrefixWarmupState(p.modId,found->second,p.name)!=1) return {};
    const std::string snapshot=prefixItemSnapshot(item);
    if (snapshot.empty()) return {};
    const std::string key=p.modId+"/"+p.name+"|"+snapshot;
    struct TooltipCache {
        std::string key;
        std::vector<std::pair<std::string,bool>> lines;
        std::chrono::steady_clock::time_point updated{};
    };
    static thread_local TooltipCache cache;
    const auto now=std::chrono::steady_clock::now();
    if (cache.key==key && now-cache.updated<std::chrono::milliseconds(200))
        return cache.lines;
    std::string wire;
    std::vector<std::pair<std::string,bool>> result;
    if (invokeManagedPrefix(p.modId,found->second,p.name,5,snapshot,&wire,650)) {
        const auto nibble=[](char ch)->int {
            if (ch>='0' && ch<='9') return ch-'0';
            if (ch>='A' && ch<='F') return ch-'A'+10;
            return -1;
        };
        size_t pos=0;
        while (pos<wire.size() && result.size()<4) {
            const auto stop=wire.find(';',pos);
            const auto end=stop==std::string::npos?wire.size():stop;
            if (end<pos+4 || wire[pos+1]!=':' ||
                (wire[pos]!='0' && wire[pos]!='1') ||
                (end-pos-2)%2 || end-pos-2>240) {
                result.clear();
                break;
            }
            std::string line;
            line.reserve((end-pos-2)/2);
            bool valid=true;
            for (size_t i=pos+2;i<end;i+=2) {
                const int hi=nibble(wire[i]),lo=nibble(wire[i+1]);
                if (hi<0 || lo<0) { valid=false;break; }
                const int ch=(hi<<4)|lo;
                if (ch<32 || ch==127) {valid=false;break;}
                line.push_back(static_cast<char>(ch));
            }
            if (!valid || line.empty()) {result.clear();break;}
            result.emplace_back(std::move(line),wire[pos]=='1');
            pos=stop==std::string::npos?end:stop+1;
            if (stop!=std::string::npos && pos==wire.size()) {result.clear();break;}
        }
        if (pos<wire.size()) result.clear();
    }
    cache.key=key;
    cache.lines=result;
    cache.updated=now;
    return result;
}

int nativeItemRollWeight(Il2CppObject* item, const ManagedPrefixEntry& p) {
    if (!item || g_refs.type < 0) return 0;
    const int itemType = bl::runtime::field<int32_t>(item, g_refs.type);
    const auto rule = p.itemWeights.find(itemType);
    const int baseWeight =
        rule == p.itemWeights.end() ? static_cast<int>(p.rollWeight) : rule->second;
    if (p.hasStackRollWeight) {
        if (g_refs.stack < 0) return 0;
        const int stack = bl::runtime::field<int32_t>(item, g_refs.stack);
        return stack >= p.minWeightStack && stack <= p.maxWeightStack
            ? p.weightInsideStackRange : baseWeight;
    }
    if (p.hasAttributeRollWeight) {
        const int offset = p.attributeWeightKind == 'D' ? g_refs.damage :
                           p.attributeWeightKind == 'M' ? g_refs.mana : -1;
        if (offset < 0) return 0;
        const int actual = bl::runtime::field<int32_t>(item, offset);
        return actual >= p.minWeightAttribute && actual <= p.maxWeightAttribute
            ? p.weightInsideAttributeRange : baseWeight;
    }
    return baseWeight;
}

int itemRollWeight(Il2CppObject* item, const ManagedPrefixEntry& p) {
    const int native=nativeItemRollWeight(item,p);
    const int managed=queryPrefixWorker(item,p,1);
    return managed<0?native:managed;
}

bool itemCanRoll(Il2CppObject* item, const ManagedPrefixEntry& p) {
    if (!item || g_refs.type < 0) return false;
    const int type = bl::runtime::field<int32_t>(item, g_refs.type);
    const auto decision = p.itemCanRoll.find(type);
    return decision == p.itemCanRoll.end() ? itemRollWeight(item, p) > 0 : decision->second;
}

bool attributeDenied(
    Il2CppObject* item, int prefix,
    const std::map<int, std::vector<ManagedPrefixAttributeDenyRange>>* rules) {
    if (!rules || !item) return false;
    const auto it = rules->find(prefix);
    if (it == rules->end()) return false;
    for (const auto& rule : it->second) {
        const int offset = rule.attribute == 'D' ? g_refs.damage :
                           rule.attribute == 'M' ? g_refs.mana : -1;
        if (offset < 0) return true;
        const int actual = bl::runtime::field<int32_t>(item, offset);
        if (actual >= rule.minimum && actual <= rule.maximum) return true;
    }
    return false;
}

bool isPrefixDenied(Il2CppObject* item, int prefix) {
    if (!item || g_refs.type < 0 || prefix <= 0) return false;
    const auto policies = managedItemPrefixDenyView();
    const int itemType = bl::runtime::field<int32_t>(item, g_refs.type);
    const auto found = policies->find(itemType);
    if (found != policies->end() && found->second.contains(prefix)) return true;
    const auto stackPolicies = managedItemPrefixStackDenyView();
    const auto itemRules = stackPolicies->find(itemType);
    if (itemRules != stackPolicies->end()) {
        const auto condition = itemRules->second.find(prefix);
        if (condition != itemRules->second.end()) {
            if (g_refs.stack < 0) return true;
            const int stack = bl::runtime::field<int32_t>(item, g_refs.stack);
            if (stack >= condition->second.minStack &&
                stack <= condition->second.maxStack) return true;
        }
    }
    const auto attributes = managedItemPrefixAttributeDenyView();
    const auto attributeItem = attributes->find(itemType);
    return attributeItem != attributes->end() &&
        attributeDenied(item, prefix, &attributeItem->second);
}

bool eligibleNative(Il2CppObject* item, const ManagedPrefixEntry& p, bool vanillaHasRollable) {
    const auto& r = g_refs;
    if (!item || !itemCanRoll(item, p)) return false;
    if (p.minItemDamage != 0 || p.maxItemDamage != 100000) {
        if (r.damage < 0) return false;
        const int damage = bl::runtime::field<int32_t>(item, r.damage);
        if (damage < p.minItemDamage || damage > p.maxItemDamage) return false;
    }
    if (p.minItemMana != 0 || p.maxItemMana != 10000) {
        if (r.mana < 0) return false;
        const int mana = bl::runtime::field<int32_t>(item, r.mana);
        if (mana < p.minItemMana || mana > p.maxItemMana) return false;
    }
    if (p.minItemStack != 0 || p.maxItemStack != 999999) {
        if (r.stack < 0) return false;
        const int stack = bl::runtime::field<int32_t>(item, r.stack);
        if (stack < p.minItemStack || stack > p.maxItemStack) return false;
    }
    if (p.category == 6) {
        const int type = bl::runtime::field<int32_t>(item, r.type);
        return p.itemWeights.contains(type);
    }
    if (p.category == 5)
        return r.accessory >= 0 && bl::runtime::field<uint8_t>(item, r.accessory) != 0;
    if (!weaponCandidate(item, vanillaHasRollable)) return false;
    if (p.category == 4) return true;
    int flag = p.category == 0 ? r.melee :
               p.category == 1 ? r.ranged :
               p.category == 2 ? r.magic :
               p.category == 3 ? r.summon : -1;
    return flag >= 0 && bl::runtime::field<uint8_t>(item, flag) != 0;
}

bool eligible(Il2CppObject* item, const ManagedPrefixEntry& p, bool vanillaHasRollable) {
    if (!eligibleNative(item,p,vanillaHasRollable)) return false;
    return queryPrefixWorker(item,p,0)!=0;
}

bool restoreCompatible(Il2CppObject* item, const ManagedPrefixEntry& p) {
    const auto& r = g_refs;
    if (!item || r.type < 0) return false;
    const int type = bl::runtime::field<int32_t>(item, r.type);
    if (p.category == 6) {
        const auto weight = p.itemWeights.find(type);
        if (weight == p.itemWeights.end()) return false;
        const auto allowed = p.itemCanRoll.find(type);
        return allowed == p.itemCanRoll.end() ? weight->second > 0 : allowed->second;
    }
    if (p.category == 5)
        return r.accessory >= 0 &&
               bl::runtime::field<uint8_t>(item, r.accessory) != 0;
    if (!weaponCandidate(item, true)) return false;
    if (p.category == 4) return true;
    const int flag = p.category == 0 ? r.melee :
                     p.category == 1 ? r.ranged :
                     p.category == 2 ? r.magic :
                     p.category == 3 ? r.summon : -1;
    return flag >= 0 && bl::runtime::field<uint8_t>(item, flag) != 0;
}

bool ensurePrefixTables(const std::vector<ManagedPrefixEntry>& prefixes) {
    if (prefixes.empty()) return true;
    auto& a = bl::il2cpp::api();
    const auto& r = g_refs;
    Il2CppArray* names = staticArray(r.names);
    Il2CppArray* chances = staticArray(r.chances);
    if (!names || !chances) return false;
    int top = 0;
    for (const auto& p : prefixes) top = std::max(top, p.type + 1);
    if (top < 1 || top > 256) return false;
    if (names->length < static_cast<uintptr_t>(top)) {
        auto* newNames = bl::runtime::TypeTables::resizedCopy(names, top);
        if (!newNames) return false;
        a.field_static_set_value(r.names, newNames);
        names = newNames;
    }
    if (chances->length < static_cast<uintptr_t>(top)) {
        auto* newChances = bl::runtime::TypeTables::resizedCopy(chances, top);
        if (!newChances) return false;
        a.field_static_set_value(r.chances, newChances);
    }
    auto** cells = static_cast<Il2CppObject**>(arrayData(names));
    for (const auto& p : prefixes) {
        if (cells[p.type]) continue;
        const std::string key = "BunnyLoader.ManagedPrefix." + bl::runtime::modPrefixKey(p.type);
        Il2CppObject* localized = bl::runtime::content::makeLocalizedText(
            key, p.displayName.empty() ? p.name : p.displayName);
        if (!localized || !bl::runtime::content::setTableElement(r.names, p.type, localized))
            return false;
    }
    return true;
}

bool hookItemPrefixChance(Il2CppObject* item, int32_t want, uint8_t* topTier,
                          const MethodInfo* method) {
    if (!g_originalItemPrefix) return false;
    int32_t request = want;
    const bool ready = g_chanceReady.load(std::memory_order_acquire) && item &&
        g_chanceItemTypeOffset >= 0 && g_chancePrefixOffset >= 0 &&
        g_chanceDamageOffset >= 0 && g_chanceValueOffset >= 0;
    const int type = ready
        ? bl::runtime::field<int32_t>(item, g_chanceItemTypeOffset) : 0;

    if (ready && want > 0 && g_restoreRoll.item != item &&
        isPrefixDenied(item, want)) return false;

    if (ready && g_restoreRoll.item != item && (want == -1 || want == -2)) {
        const auto stackPolicies = managedItemPrefixStackChanceView();
        const auto stackRule = stackPolicies->find(type);
        bool useStackDecision = false;
        bool allow = false;
        if (stackRule != stackPolicies->end()) {
            if (g_refs.stack < 0) return false;
            const int stack = bl::runtime::field<int32_t>(item, g_refs.stack);
            if (stack >= stackRule->second.minStack &&
                stack <= stackRule->second.maxStack) {
                useStackDecision = true;
                allow = stackRule->second.allowed;
            }
        }
        if (!useStackDecision) {
            const auto chances = managedItemPrefixChanceView();
            const auto decision = chances->find(type);
            if (decision != chances->end()) {
                useStackDecision = true;
                allow = decision->second;
            }
        }
        if (useStackDecision) {
            if (!allow) return false;
            if (want == -1) request = -2;
        }
    }

    const auto effects = ready && want != -3 ? managedItemPrefixApplyView() : nullptr;
    const auto policy = effects ? effects->find(type) :
        std::map<int, ManagedItemPrefixApply>::const_iterator{};
    const bool hasItemPolicy = effects && policy != effects->end();
    const auto prefixView = ready && want != -3 ? managedPrefixView() : nullptr;
    const int oldPrefix = ready ? bl::runtime::field<uint8_t>(item, g_chancePrefixOffset) : 0;
    const auto* oldPrefixPolicy = prefixView ? byType(*prefixView, oldPrefix) : nullptr;
    const bool hadPrefixBonus = oldPrefixPolicy &&
        (oldPrefixPolicy->applyDamageBonus || oldPrefixPolicy->applyValueBonus ||
         (oldPrefixPolicy->callbackMask & 8));
    auto* previous = ready ? rememberedPrefix(item) : nullptr;
    bool removedPrevious = false;
    int originalDamage = 0;
    int originalValue = 0;
    if (previous && want != -3) {
        const int currentDamage = bl::runtime::field<int32_t>(item, g_chanceDamageOffset);
        const int currentValue = bl::runtime::field<int32_t>(item, g_chanceValueOffset);
        const int currentPrefix = bl::runtime::field<uint8_t>(item, g_chancePrefixOffset);
        if ((hasItemPolicy || hadPrefixBonus) &&
            previous->type == type && previous->prefix == currentPrefix &&
            previous->damage == currentDamage && previous->value == currentValue &&
            currentDamage >= previous->damageBonus &&
            currentValue >= previous->valueBonus) {
            originalDamage = currentDamage;
            originalValue = currentValue;
            bl::runtime::field<int32_t>(item, g_chanceDamageOffset) =
                currentDamage - previous->damageBonus;
            bl::runtime::field<int32_t>(item, g_chanceValueOffset) =
                currentValue - previous->valueBonus;
            removedPrevious = true;
        } else {
            previous->item = nullptr;
        }
    }

    const bool accepted = g_originalItemPrefix(item, request, topTier, method);
    if (!accepted || !ready || want == -3) {
        if (removedPrevious && !accepted) {
            if (bl::runtime::field<int32_t>(item, g_chanceDamageOffset) ==
                    originalDamage - previous->damageBonus &&
                bl::runtime::field<int32_t>(item, g_chanceValueOffset) ==
                    originalValue - previous->valueBonus) {
                bl::runtime::field<int32_t>(item, g_chanceDamageOffset) = originalDamage;
                bl::runtime::field<int32_t>(item, g_chanceValueOffset) = originalValue;
            } else {
                previous->item = nullptr;
            }
        }
        return accepted;
    }

    const int prefix = bl::runtime::field<uint8_t>(item, g_chancePrefixOffset);
    const auto* prefixPolicy = prefixView ? byType(*prefixView, prefix) : nullptr;
    int callbackDamage=0,callbackValue=0;
    if (prefixPolicy) queryPrefixApply(item,*prefixPolicy,&callbackDamage,&callbackValue);
    const int damageBonus = (hasItemPolicy ? policy->second.damageBonus : 0) +
        (prefixPolicy ? prefixPolicy->applyDamageBonus : 0) + callbackDamage;
    const int valueBonus = (hasItemPolicy ? policy->second.valueBonus : 0) +
        (prefixPolicy ? prefixPolicy->applyValueBonus : 0) + callbackValue;
    if (!prefix || (damageBonus == 0 && valueBonus == 0)) {
        if (previous) previous->item = nullptr;
        return accepted;
    }
    const int damage = bl::runtime::field<int32_t>(item, g_chanceDamageOffset);
    const int value = bl::runtime::field<int32_t>(item, g_chanceValueOffset);
    if (damage < 0 || value < 0 ||
        damage > INT32_MAX - damageBonus ||
        value > INT32_MAX - valueBonus) {
        if (previous) previous->item = nullptr;
        return accepted;
    }
    const int appliedDamage = damage + damageBonus;
    const int appliedValue = value + valueBonus;
    bl::runtime::field<int32_t>(item, g_chanceDamageOffset) = appliedDamage;
    bl::runtime::field<int32_t>(item, g_chanceValueOffset) = appliedValue;
    rememberPrefix(item, type, prefix, appliedDamage, appliedValue,
                   {damageBonus, valueBonus});
    return accepted;
}

Il2CppArray* hookRollable(Il2CppObject* item, const MethodInfo* method) {
    Il2CppArray* base = g_originalRollable ? g_originalRollable(item, method) : nullptr;
    if (!g_hooksReady || !item) return base;
    const auto descriptors = managedPrefixView();
    const auto bans = managedItemPrefixDenyView();
    const int itemType = g_refs.type >= 0 ?
        bl::runtime::field<int32_t>(item, g_refs.type) : -1;
    const auto ban = bans->find(itemType);
    const std::set<int>* denied = ban == bans->end() ? nullptr : &ban->second;
    const auto stackBans = managedItemPrefixStackDenyView();
    const auto stackBan = stackBans->find(itemType);
    const std::map<int, ManagedPrefixStackDenyRange>* conditional =
        stackBan == stackBans->end() ? nullptr : &stackBan->second;
    const int currentStack = conditional && g_refs.stack >= 0
        ? bl::runtime::field<int32_t>(item, g_refs.stack) : -1;
    const auto attributeBans = managedItemPrefixAttributeDenyView();
    const auto attributeBan = attributeBans->find(itemType);
    const std::map<int, std::vector<ManagedPrefixAttributeDenyRange>>* attributes =
        attributeBan == attributeBans->end() ? nullptr : &attributeBan->second;
    const auto allowed = [denied, conditional, currentStack, attributes, item](int prefix) {
        if (denied && denied->contains(prefix)) return false;
        if (conditional) {
            const auto found = conditional->find(prefix);
            if (found != conditional->end() &&
                (currentStack < 0 ||
                 (currentStack >= found->second.minStack &&
                  currentStack <= found->second.maxStack))) return false;
        }
        return !attributeDenied(item, prefix, attributes);
    };
    const bool tablesReady = descriptors->empty() || ensurePrefixTables(*descriptors);
    const bool vanillaHasRollable = base && base->length > 0;
    const uintptr_t oldCount = base ? base->length : 0;
    const auto* oldValues = oldCount ?
        static_cast<const int32_t*>(arrayData(base)) : nullptr;
    if (!tablesReady) {
        for (const auto& p : *descriptors) {
            if (p.category != 6 && allowed(p.type) &&
                eligible(item, p, vanillaHasRollable) && itemRollWeight(item, p) > 0) {
                g_failedRollableItem = item;
                return nullptr;
            }
        }
    }
    uintptr_t retained = 0;
    for (uintptr_t i = 0; i < oldCount; ++i)
        if (allowed(oldValues[i])) ++retained;
    int extra = 0;
    for (const auto& p : *descriptors)
        if (tablesReady && p.category != 6 && allowed(p.type) &&
            eligible(item, p, vanillaHasRollable))
            extra += itemRollWeight(item, p);
    if (retained == oldCount && extra == 0) return base;
    const uintptr_t total = retained + static_cast<uintptr_t>(extra);
    if (total == 0) return nullptr;
    if (total > 512 || !g_refs.intClass) {
        g_failedRollableItem = item;
        return nullptr;
    }
    Il2CppArray* out = bl::il2cpp::api().array_new(g_refs.intClass, total);
    if (!out) {
        g_failedRollableItem = item;
        return nullptr;
    }
    auto* values = static_cast<int32_t*>(arrayData(out));
    uintptr_t index = 0;
    for (uintptr_t i = 0; i < oldCount; ++i)
        if (allowed(oldValues[i])) values[index++] = oldValues[i];
    for (const auto& p : *descriptors)
        if (tablesReady && p.category != 6 && allowed(p.type) &&
            eligible(item, p, vanillaHasRollable))
            for (int i = 0; i < itemRollWeight(item, p); ++i)
                values[index++] = p.type;
    return out;
}

bool hookRoll(Il2CppObject* item, Il2CppObject* random, int32_t* rolled,
              const MethodInfo* method) {
    PrefixRollDuration duration;
    g_prefixRollAttempts.fetch_add(1, std::memory_order_relaxed);
    if (!g_hooksReady.load(std::memory_order_acquire) ||
        !item || !random || !rolled || !g_refs.rollableMethod ||
        !g_refs.nextDoubleMethod)
        return nativeRollFallback(item, random, rolled, method);

    const auto descriptors = managedPrefixView();
    if (g_restoreRoll.item == item && g_restoreRoll.prefix > 0) {
        if (const auto* target = byType(*descriptors, g_restoreRoll.prefix)) {
            if (restoreCompatible(item, *target) && ensurePrefixTables(*descriptors)) {
                *rolled = target->type;
                g_restoreRoll.used = true;
                g_prefixRestoredRolls.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
    }
    const auto banView = managedItemPrefixDenyView();
    const int currentItemType = g_refs.type >= 0 ?
        bl::runtime::field<int32_t>(item, g_refs.type) : -1;
    bool hasDeny = banView->contains(currentItemType);
    if (!hasDeny) {
        const auto stackBans = managedItemPrefixStackDenyView();
        const auto found = stackBans->find(currentItemType);
        if (found != stackBans->end())
            for (const auto& [prefix, range] : found->second)
                if (isPrefixDenied(item, prefix)) {
                    hasDeny = true;
                    break;
                }
    }
    if (!hasDeny) {
        const auto attributeBans = managedItemPrefixAttributeDenyView();
        const auto found = attributeBans->find(currentItemType);
        if (found != attributeBans->end())
            for (const auto& [prefix, rules] : found->second)
                if (attributeDenied(item, prefix, &found->second)) {
                    hasDeny = true;
                    break;
                }
    }
    PrefixRpcScope prefixCallbacks;
    if (g_restoreRoll.item != item && currentItemType > 0) {
        const auto choices = managedItemPrefixChoiceView();
        const auto selected = choices->find(currentItemType);
        if (selected != choices->end() && g_refs.stack >= 0) {
            const int stack = bl::runtime::field<int32_t>(item, g_refs.stack);
            if (stack >= selected->second.minStack &&
                stack <= selected->second.maxStack) {
                const int choice = selected->second.prefix;
                if (!isPrefixDenied(item, choice)) {
                    const auto* managed = byType(*descriptors, choice);
                    const bool allowed = managed
                        ? eligible(item, *managed, true) && ensurePrefixTables(*descriptors)
                        : g_originalCanRoll && g_originalCanRoll(item, choice, nullptr);
                    if (allowed) {
                        *rolled = choice;
                        g_prefixChosenRolls.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                }
            }
        }
    }
    if (descriptors->empty() && !hasDeny)
        return nativeRollFallback(item, random, rolled, method);
    if (g_restoreRoll.item != item && g_refs.type >= 0) {
        const int type = bl::runtime::field<int32_t>(item, g_refs.type);
        for (const auto& policy : *descriptors) {
            if (policy.preferredTypes.contains(type) && !isPrefixDenied(item, policy.type) &&
                eligible(item, policy, true) &&
                ensurePrefixTables(*descriptors)) {
                *rolled = policy.type;
                g_prefixPreferredRolls.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
    }

    auto& api = bl::il2cpp::api();
    g_failedRollableItem = nullptr;
    Il2CppObject* error = nullptr;
    auto* list = reinterpret_cast<Il2CppArray*>(
        api.runtime_invoke(g_refs.rollableMethod, item, nullptr, &error));
    const bool managedCandidateFailure = g_failedRollableItem == item;
    g_failedRollableItem = nullptr;
    if (error || !list || list->length == 0 || list->length > 512)
        return (hasDeny || managedCandidateFailure) ? false :
            (nativeRollFallback(item, random, rolled, method));

    const auto* values = static_cast<const int32_t*>(arrayData(list));
    bool hasManaged = false;
    for (uintptr_t i = 0; i < list->length; ++i)
        if (byType(*descriptors, values[i])) {
            hasManaged = true;
            break;
        }
    if (!hasManaged && !hasDeny)
        return nativeRollFallback(item, random, rolled, method);

    error = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(
        g_refs.nextDoubleMethod, random, nullptr, &error);
    if (error || !boxed) return false;
    double chance = 0.0;
    std::memcpy(&chance,
                reinterpret_cast<const char*>(boxed) + sizeof(Il2CppObject),
                sizeof(chance));
    if (!std::isfinite(chance) || chance < 0.0 || chance >= 1.0)
        return false;

    const int chosen = weightedPrefixCandidate(
        values, static_cast<size_t>(list->length), chance);
    if (chosen < 0) {
        g_prefixRejectedRolls.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    *rolled = chosen;
    g_prefixWeightedRolls.fetch_add(1, std::memory_order_relaxed);
    const int reported = g_reforgeDiagnostics.fetch_add(1, std::memory_order_relaxed);
    if (reported < 8)
        BL_INFO("csharp prefix: RollAPrefix selecionou %d de %zu candidatos (%d gerenciados)",
                *rolled, static_cast<size_t>(list->length),
                static_cast<int>(descriptors->size()));
    return true;
}

bool hookCanRoll(Il2CppObject* item, int32_t prefix, const MethodInfo* method) {
    if (g_hooksReady && g_restoreRoll.item != item &&
        isPrefixDenied(item, prefix)) return false;
    if (g_hooksReady) {
        const auto list = managedPrefixView();
        if (const auto* p = byType(*list, prefix)) {
            if (g_restoreRoll.item == item && g_restoreRoll.prefix == prefix)
                return restoreCompatible(item,*p) && ensurePrefixTables(*list);
            PrefixRpcScope prefixCallbacks;
            return eligible(item,*p,true) && ensurePrefixTables(*list);
        }
    }
    return g_originalCanRoll ? g_originalCanRoll(item, prefix, method) : false;
}

bool hookStats(Il2CppObject* item, int32_t prefix, float* dmg, float* kb,
               float* spd, float* size, float* shootSpeed, float* mana,
               int32_t* crit, int32_t* tagDamage, int32_t* pen,
               float* value, const MethodInfo* method) {
    if (g_hooksReady && item) {
        const auto list = managedPrefixView();
        if (const auto* p = byType(*list, prefix)) {
            PrefixRpcScope prefixCallbacks;
            const bool restoring = g_restoreRoll.item == item &&
                g_restoreRoll.prefix == prefix && restoreCompatible(item, *p);
            if (!dmg || !kb || !spd || !size || !shootSpeed || !mana ||
                !crit || !tagDamage || !pen || !value ||
                !ensurePrefixTables(*list) ||
                (!restoring && !eligible(item, *p, true)) ||
                (!restoring && queryPrefixWorker(item,*p,3)==0))
                return false;
            *dmg = p->multipliers[0];
            *kb = p->multipliers[1];
            *spd = p->multipliers[2];
            *size = p->multipliers[3];
            *shootSpeed = p->multipliers[4];
            *mana = p->multipliers[5];
            *crit = p->crit;
            *tagDamage = p->tagDamage;
            *pen = p->armorPenetration;
            *value = p->valueMultiplier;
            const auto& r = g_refs;
            const auto itemDamage = bl::runtime::field<int32_t>(item, r.damage);
            const auto itemAnimation = bl::runtime::field<int32_t>(item, r.useAnimation);
            const auto itemMana = bl::runtime::field<int32_t>(item, r.mana);
            const auto itemKnockBack = bl::runtime::field<float>(item, r.knockBack);
            const auto changes = [](int current, float mult) {
                return mult == 1.f ||
                    std::nearbyint(static_cast<float>(current) * mult) != static_cast<float>(current);
            };
            return restoring || !p->requireEffectiveStats ||
                   (changes(itemDamage, *dmg) && changes(itemAnimation, *spd) &&
                    changes(itemMana, *mana) && (*kb == 1.f || itemKnockBack != 0.f));
        }
    }
    return g_originalStats ? g_originalStats(item, prefix, dmg, kb, spd, size,
                                            shootSpeed, mana, crit, tagDamage,
                                            pen, value, method) : false;
}

int queryPrefixAccessory(Il2CppObject* player,const ManagedPrefixEntry& p) {
    if (!(p.callbackMask & 32) || p.type<1 || p.type>255 ||
        g_refs.playerLife<0 || g_refs.playerLifeMax<0) return 0;
    const int life=bl::runtime::field<int32_t>(player,g_refs.playerLife);
    const int maximum=bl::runtime::field<int32_t>(player,g_refs.playerLifeMax);
    if (life<0 || maximum<1 || maximum>1000000 || life>maximum) return 0;
    struct CachedBonus {
        Il2CppObject* player=nullptr;
        int life=-1;
        int maximum=-1;
        int bonus=0;
        std::chrono::steady_clock::time_point checked{};
    };
    static thread_local std::array<CachedBonus,256> cache{};
    static thread_local std::chrono::steady_clock::time_point lastRequest{};
    auto& found=cache[static_cast<size_t>(p.type)];
    const auto now=std::chrono::steady_clock::now();
    if (found.player==player && found.life==life && found.maximum==maximum &&
        now-found.checked<std::chrono::milliseconds(250)) return found.bonus;
    found={player,life,maximum,0,now};
    if (now-lastRequest<std::chrono::milliseconds(150)) return 0;
    const auto assemblies=managedPrefixCallbackAssemblies();
    const auto assembly=assemblies->find(p.modId);
    if (assembly==assemblies->end() ||
        managedPrefixWarmupState(p.modId,assembly->second,p.name)!=1) return 0;
    lastRequest=now;
    std::string answer;
    if (!invokeManagedPrefix(p.modId,assembly->second,p.name,6,
            std::to_string(life)+","+std::to_string(maximum),&answer,650)) return 0;
    int sign=1;
    size_t first=0;
    if (!answer.empty() && answer.front()=='-') {sign=-1;first=1;}
    if (first==answer.size()) return 0;
    int bonus=0;
    for (size_t i=first;i<answer.size();++i) {
        if (answer[i]<'0'||answer[i]>'9') return 0;
        bonus=bonus*10+(answer[i]-'0');
        if (bonus>100) return 0;
    }
    found.bonus=bonus*sign;
    return found.bonus;
}

void hookGrant(Il2CppObject* player, Il2CppObject* item, const MethodInfo* method) {
    if (g_originalGrant) g_originalGrant(player, item, method);
    if (!g_hooksReady || !player || !item) return;
    const int prefix = bl::runtime::field<uint8_t>(item, g_refs.prefix);
    const auto list = managedPrefixView();
    const auto* found = byType(*list, prefix);
    if (!found || found->category != 5) return;
    const int bonus=found->accessoryDefense+queryPrefixAccessory(player,*found);
    const int64_t candidate=static_cast<int64_t>(
        bl::runtime::field<int32_t>(player,g_refs.playerDefense))+bonus;
    if (candidate>=INT32_MIN && candidate<=INT32_MAX)
        bl::runtime::field<int32_t>(player,g_refs.playerDefense)=static_cast<int32_t>(candidate);
}

std::string utf8Text(const Il2CppString* input) {
    std::string out;
    if (!input || input->length < 0 || input->length > 256) return out;
    out.reserve(static_cast<size_t>(input->length) * 3);
    for (int i = 0; i < input->length; ++i) {
        uint32_t point = input->chars[i];
        if (point >= 0xD800 && point <= 0xDBFF && i + 1 < input->length) {
            const uint32_t next = input->chars[i + 1];
            if (next >= 0xDC00 && next <= 0xDFFF) {
                point = 0x10000 + ((point - 0xD800) << 10) + next - 0xDC00;
                ++i;
            }
        }
        if (point >= 0xD800 && point <= 0xDFFF) continue;
        if (point < 0x80) out.push_back(static_cast<char>(point));
        else if (point < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (point >> 6)));
            out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        } else if (point < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (point >> 12)));
            out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (point >> 18)));
            out.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        }
    }
    return out;
}

std::string accessoryStatSuffix(int index, const char* fallback) {
    const auto& r = g_refs;
    const auto& api = bl::il2cpp::api();
    auto* tips = staticArray(r.tooltipSuffixes);
    if (!tips || static_cast<uintptr_t>(index) >= tips->length ||
        !r.localizedValueMethod) return fallback;
    auto** texts = static_cast<Il2CppObject**>(arrayData(tips));
    Il2CppObject* entry = texts[index];
    if (!entry) return fallback;
    Il2CppObject* error = nullptr;
    Il2CppObject* value = api.runtime_invoke(r.localizedValueMethod, entry, nullptr, &error);
    if (error || !value) return fallback;
    std::string localized = utf8Text(reinterpret_cast<Il2CppString*>(value));
    return localized.empty() ? fallback : localized;
}

std::string accessoryDefenseSuffix() {
    return accessoryStatSuffix(25, " defense");
}

std::string accessoryMoveSpeedSuffix() {
    return accessoryStatSuffix(46, "% movement speed");
}

void hookReforgeTooltip(Il2CppObject* popup, Il2CppObject* item, const MethodInfo* method) {
    if (g_originalReforge) g_originalReforge(popup, item, method);
    if (!g_reforgeTooltipReady.load(std::memory_order_acquire) || !popup || !item) return;
    const auto& r = g_refs;
    const int prefix = bl::runtime::field<uint8_t>(item, r.prefix);
    const auto prefixes = managedPrefixView();
    const auto* p = byType(*prefixes, prefix);
    if (!p) return;
    const bool defense = p->category==5 && p->accessoryDefense!=0 &&
        eligible(item,*p,true);
    const bool dynamic = (p->callbackMask & 16)!=0;
    if (!defense && !dynamic) return;
    auto* text = bl::runtime::field<Il2CppArray*>(popup, r.reforgeText);
    auto* bad = bl::runtime::field<Il2CppArray*>(popup, r.reforgeBad);
    if (!text || !bad) return;
    const uintptr_t capacity=std::min(text->length,bad->length);
    if (!capacity || capacity>128) return;
    int count=bl::runtime::field<int32_t>(popup,r.reforgeNumLines);
    if (count<0 || static_cast<uintptr_t>(count)>capacity) return;
    auto& api=bl::il2cpp::api();
    if (!api.string_new || !api.gc_wbarrier_set_field) return;
    auto** strings=static_cast<Il2CppString**>(arrayData(text));
    auto* negative=static_cast<uint8_t*>(arrayData(bad));
    const auto append=[&](int index,const std::string& line,bool isBad) {
        if (index<0 || static_cast<uintptr_t>(index)>=capacity || line.empty())
            return false;
        auto* value=api.string_new(line.c_str());
        if (!value) return false;
        api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(text),
            reinterpret_cast<void**>(&strings[index]),value);
        negative[index]=isBad?1:0;
        return true;
    };
    if (defense && count<=1) {
        const int bonus=p->accessoryDefense;
        const std::string description=(bonus>0?"+":"")+
            std::to_string(bonus)+accessoryDefenseSuffix();
        if (append(0,description,bonus<0)) count=1;
    }
    if (dynamic)
        for (const auto& line:queryPrefixTooltip(item,*p)) {
            if (static_cast<uintptr_t>(count)>=capacity) break;
            if (append(count,line.first,line.second)) ++count;
        }
    bl::runtime::field<int32_t>(popup,r.reforgeNumLines)=count;
}

void hookInventoryTooltip(Il2CppObject* item, int32_t* yoyo, int32_t* research,
                          int32_t* materials, float oldKb, int32_t* count,
                          Il2CppArray* lines, Il2CppArray* modifier,
                          Il2CppArray* bad, int32_t* setBonus, void* setColor,
                          int32_t* shared, const MethodInfo* method) {
    if (g_originalInventoryTooltip)
        g_originalInventoryTooltip(item, yoyo, research, materials, oldKb, count,
                                   lines, modifier, bad, setBonus, setColor,
                                   shared, method);
    if (g_inventoryTooltipReady.load(std::memory_order_acquire) && item &&
        count && lines && modifier && bad && g_refs.prefix >= 0) {
        const int prefixId = bl::runtime::field<uint8_t>(item, g_refs.prefix);
        const auto prefixes = managedPrefixView();
        const auto* selected = byType(*prefixes, prefixId);
        const uintptr_t capacity = std::min({lines->length, modifier->length, bad->length});
        if (selected && *count >= 0 && static_cast<uintptr_t>(*count) < capacity) {
            auto& api = bl::il2cpp::api();
            if (api.string_new && api.gc_wbarrier_set_field) {
                auto** output = static_cast<Il2CppString**>(arrayData(lines));
                auto* flags = static_cast<uint8_t*>(arrayData(modifier));
                auto* negative = static_cast<uint8_t*>(arrayData(bad));
                const auto append = [&](const std::string& message,
                                        bool isNegative=false) {
                    if (message.empty() || static_cast<uintptr_t>(*count) >= capacity)
                        return;
                    Il2CppString* rendered = api.string_new(message.c_str());
                    if (!rendered) return;
                    api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(lines),
                        reinterpret_cast<void**>(&output[*count]), rendered);
                    flags[*count] = 1;
                    negative[*count] = isNegative?1:0;
                    ++*count;
                };
                append(selected->extraTooltip);
                for (const auto& line : selected->tooltipLines)
                    append(line);
                if (!selected->tooltipItemLines.empty() &&
                    g_refs.damage >= 0 && g_refs.mana >= 0 && g_refs.stack >= 0) {
                    const int damage = bl::runtime::field<int32_t>(item, g_refs.damage);
                    const int mana = bl::runtime::field<int32_t>(item, g_refs.mana);
                    const int stack = bl::runtime::field<int32_t>(item, g_refs.stack);
                    for (const auto& itemLine : selected->tooltipItemLines)
                        append(renderPrefixItemTooltip(itemLine, damage, mana, stack));
                }
                for (const auto& line : queryPrefixTooltip(item,*selected))
                    append(line.first,line.second);
            }
        }
    }
    if (!g_inventoryTooltipReady.load(std::memory_order_acquire) || !item ||
        !count || !lines || !modifier || !bad ||
        g_refs.accessory < 0 || g_refs.type < 0 || g_refs.prefix < 0 ||
        !bl::runtime::field<uint8_t>(item, g_refs.accessory))
        return;

    const int itemType = bl::runtime::field<int32_t>(item, g_refs.type);
    const int baseDefense = managedNativeAccessoryDefense(itemType);
    const int moveSpeed = managedNativeAccessoryMoveSpeedPercent(itemType);
    if (baseDefense <= 0 && moveSpeed <= 0) return;

    const int prefixType = bl::runtime::field<uint8_t>(item, g_refs.prefix);
    const auto prefixes = managedPrefixView();
    const auto* current = byType(*prefixes, prefixType);
    const int prefixDefense = current && current->category == 5
        ? current->accessoryDefense : 0;

    const auto capacity = std::min({lines->length, modifier->length, bad->length});
    if (*count < 0 || static_cast<uintptr_t>(*count) >= capacity) return;
    auto& api = bl::il2cpp::api();
    if (!api.string_new || !api.gc_wbarrier_set_field) return;

    auto** descriptions = static_cast<Il2CppString**>(arrayData(lines));
    auto* flags = static_cast<uint8_t*>(arrayData(modifier));
    auto* negative = static_cast<uint8_t*>(arrayData(bad));
    const std::string defenseSuffix = accessoryDefenseSuffix();

    const auto append = [&](int amount, const std::string& suffix,
                            const std::string& label, bool prefixLine) {
        if (amount == 0 || static_cast<uintptr_t>(*count) >= capacity) return;
        const std::string value = (amount > 0 ? "+" : "") + std::to_string(amount) +
                                  suffix + " (" + label + ")";
        Il2CppString* line = api.string_new(value.c_str());
        if (!line) return;
        api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(lines),
            reinterpret_cast<void**>(&descriptions[*count]), line);
        flags[*count] = prefixLine ? 1 : 0;
        negative[*count] = amount < 0 ? 1 : 0;
        ++*count;
    };

    append(baseDefense, defenseSuffix, "base", false);
    if (prefixDefense)
        append(prefixDefense, defenseSuffix, current->displayName.empty()
            ? current->name : current->displayName, true);
    if (moveSpeed > 0)
        append(moveSpeed, accessoryMoveSpeedSuffix(), "base", false);
}

bool installInventoryTooltipHook() {
    using namespace bl::il2cpp;
    auto& api = bl::il2cpp::api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const auto* getLines = main ? findMethodBySignature(main, parseSignature(
        "void MouseText_DrawItemTooltip_GetLinesInfo(Item item, ref int yoyoLogo, ref int researchLine, ref int materialsLine, float oldKB, ref int numLines, string[] toolTipLine, bool[] preFixLine, bool[] badPreFixLine, ref int setBonusLine, ref Color setBonusColour, ref int sharedLine)"))
        : nullptr;
    if (!getLines || !api.string_new || !api.gc_wbarrier_set_field)
        return false;

    if (!g_refs.tooltipSuffixes)
        g_refs.tooltipSuffixes = findField(findClass({"Terraria", "Lang", {}}), "tip");
    if (!g_refs.localizedValueMethod) {
        Il2CppClass* localized = findClass({"Terraria.Localization", "LocalizedText", {}});
        g_refs.localizedValueMethod = localized
            ? api.class_get_method_from_name(localized, "get_Value", 0) : nullptr;
    }
    if (!bl::hook::install(getLines, hookInventoryTooltip, &g_originalInventoryTooltip))
        return false;
    g_inventoryTooltipReady.store(true, std::memory_order_release);
    return true;
}

bool installReforgeTooltipHook() {
    using namespace bl::il2cpp;
    auto& api = bl::il2cpp::api();
    Il2CppClass* popup = nullptr;
    if (!api.gameImage || !api.image_get_class_count || !api.image_get_class ||
        !api.class_get_name) return false;
    const size_t count = api.image_get_class_count(api.gameImage);
    for (size_t i = 0; i < count; ++i) {
        Il2CppClass* current = api.image_get_class(api.gameImage, i);
        if (current && std::strcmp(api.class_get_name(current), "GUIReforgePopup") == 0) {
            popup = current;
            break;
        }
    }
    if (!popup) return false;
    const auto* update = findMethodBySignature(
        popup, parseSignature("void UpdatePrefixes(Item reforgeItem)"));
    const int numLines = fieldOffset(popup, "numLines");
    const int text = fieldOffset(popup, "toolTipLine");
    const int bad = fieldOffset(popup, "badPreFixLine");
    auto* localized = findClass({"Terraria.Localization", "LocalizedText", {}});
    const auto* getter = localized
        ? api.class_get_method_from_name(localized, "get_Value", 0) : nullptr;
    FieldInfo* suffixes = findField(findClass({"Terraria", "Lang", {}}), "tip");
    if (!update || numLines < 0 || text < 0 || bad < 0 || !getter || !suffixes ||
        !api.string_new || !api.gc_wbarrier_set_field) return false;
    g_refs.reforgeNumLines = numLines;
    g_refs.reforgeText = text;
    g_refs.reforgeBad = bad;
    g_refs.tooltipSuffixes = suffixes;
    g_refs.localizedValueMethod = getter;
    if (!bl::hook::install(update, hookReforgeTooltip, &g_originalReforge)) return false;
    g_reforgeTooltipReady.store(true, std::memory_order_release);
    return true;
}

}
 
bool applyManagedPrefixViaNativeRoll(Il2CppObject* item, int prefix,
                                     const MethodInfo* itemPrefixMethod,
                                     bool* hookReached) {
    if (hookReached) *hookReached = false;
    if (!g_hooksReady.load(std::memory_order_acquire) || !item ||
        !itemPrefixMethod || g_restoreRoll.item) return false;
    const auto descriptors = managedPrefixView();
    const auto* requested = byType(*descriptors, prefix);
    if (!requested || !restoreCompatible(item, *requested) ||
        !ensurePrefixTables(*descriptors)) return false;
    const RestoreRoll previous = g_restoreRoll;
    g_restoreRoll = {item, prefix, false};
    int32_t request = -2;
    uint8_t topTier = 0;
    void* arguments[2] = {&request, &topTier};
    Il2CppObject* error = nullptr;
    Il2CppObject* result = bl::il2cpp::api().runtime_invoke(
        itemPrefixMethod, item, arguments, &error);
    const bool called = g_restoreRoll.used;
    g_restoreRoll = previous;
    if (hookReached) *hookReached = called;
    const bool accepted = result &&
        *reinterpret_cast<const uint8_t*>(
            reinterpret_cast<const char*>(result) + sizeof(Il2CppObject)) != 0;
    return !error && called && accepted &&
           bl::runtime::field<uint8_t>(item, g_refs.prefix) == prefix;
}

bool applyManagedPrefixStatFallback(Il2CppObject* item, int prefix, std::string* reason) {
    const auto reject = [reason](const char* why) {
        if (reason) *reason = why;
        return false;
    };
    if (!item || !g_hooksReady.load(std::memory_order_acquire))
        return reject("NativePrefixHooksNotReady");
    const auto descriptors = managedPrefixView();
    const auto* policy = byType(*descriptors, prefix);
    if (!policy) return reject("PrefixDescriptorMissing");
    if (!restoreCompatible(item, *policy)) return reject("ItemCategoryIncompatible");

    if (g_refs.prefix < 0 || g_refs.damage < 0) return reject("ItemFieldMissing");
    if (bl::runtime::field<uint8_t>(item, g_refs.prefix) != 0)
        return reject("ItemPrefixMustBeReset");
    for (size_t i = 1; i < policy->multipliers.size(); ++i)
        if (policy->multipliers[i] != 1.f)
            return reject("UnsupportedFallbackMultiplier");
    if (policy->crit || policy->tagDamage || policy->armorPenetration)
        return reject("UnsupportedFallbackStat");
    if (policy->category == 5 && policy->multipliers[0] != 1.f)
        return reject("UnsupportedAccessoryDamageMultiplier");

    auto* itemClass = bl::il2cpp::findClass({"Terraria", "Item", {}});
    const int valueOffset = itemClass ? bl::il2cpp::fieldOffset(itemClass, "value") : -1;
    if (valueOffset < 0) return reject("ItemValueFieldMissing");
    const int oldDamage = bl::runtime::field<int32_t>(item, g_refs.damage);
    const int oldValue = bl::runtime::field<int32_t>(item, valueOffset);
    const double scaledDamage = static_cast<double>(oldDamage) * policy->multipliers[0];
    const double scaledValue = static_cast<double>(oldValue) * policy->valueMultiplier;
    if (oldDamage < 0 || oldValue < 0 || !std::isfinite(scaledDamage) ||
        !std::isfinite(scaledValue) || scaledDamage > INT32_MAX ||
        scaledValue > INT32_MAX || scaledDamage < 0 || scaledValue < 0)
        return reject("NativeStatOverflow");
    if (policy->category != 5 && oldDamage <= 0)
        return reject("WeaponDamageMissing");

    bl::runtime::field<int32_t>(item, g_refs.damage) =
        static_cast<int32_t>(std::round(scaledDamage));
    bl::runtime::field<int32_t>(item, valueOffset) =
        static_cast<int32_t>(std::round(scaledValue));
    bl::runtime::field<uint8_t>(item, g_refs.prefix) = static_cast<uint8_t>(prefix);
    return true;
}

ManagedPrefixDiagnostics managedPrefixDiagnostics() {
    ManagedPrefixDiagnostics out;
    out.rollAttempts = g_prefixRollAttempts.load(std::memory_order_relaxed);
    out.restoredRolls = g_prefixRestoredRolls.load(std::memory_order_relaxed);
    out.chosenItemRolls = g_prefixChosenRolls.load(std::memory_order_relaxed);
    out.preferredRolls = g_prefixPreferredRolls.load(std::memory_order_relaxed);
    out.weightedRolls = g_prefixWeightedRolls.load(std::memory_order_relaxed);
    out.vanillaFallbacks = g_prefixVanillaFallbacks.load(std::memory_order_relaxed);
    out.rejectedRolls = g_prefixRejectedRolls.load(std::memory_order_relaxed);
    out.totalRollMicroseconds = g_prefixRollTotalUs.load(std::memory_order_relaxed);
    out.maxRollMicroseconds = g_prefixRollMaxUs.load(std::memory_order_relaxed);
    return out;
}

bool managedPrefixGameplayReady() {
    return g_hooksReady.load(std::memory_order_acquire) &&
           !snapshotManagedPrefixes().empty();
}

bool installManagedPrefixChanceHook() {
    std::lock_guard<std::mutex> guard(g_chanceInstallMutex);
    if (g_chanceReady.load(std::memory_order_acquire)) return true;
    auto* item = bl::il2cpp::findClass({"Terraria", "Item", {}});
    if (!item) return false;
    const int typeOffset = bl::il2cpp::fieldOffset(item, "type");
    const int prefixOffset = bl::il2cpp::fieldOffset(item, "prefix");
    const int damageOffset = bl::il2cpp::fieldOffset(item, "damage");
    const int valueOffset = bl::il2cpp::fieldOffset(item, "value");
    const auto* method = bl::il2cpp::findMethodBySignature(
        item, bl::il2cpp::parseSignature(
            "bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)"));
    if (typeOffset < 0 || prefixOffset < 0 || damageOffset < 0 ||
        valueOffset < 0 || !method ||
        !bl::hook::install(method, hookItemPrefixChance, &g_originalItemPrefix))
        return false;
    g_chancePrefixOffset = prefixOffset;
    g_chanceDamageOffset = damageOffset;
    g_chanceValueOffset = valueOffset;
    g_chanceItemTypeOffset = typeOffset;
    g_chanceReady.store(true, std::memory_order_release);
    BL_INFO("csharp prefix: PrefixChance nativo por item instalado");
    return true;
}

bool installManagedPrefixGameplayHooks() {
    std::lock_guard<std::mutex> guard(g_installMutex);
    if (g_hooksReady.load(std::memory_order_acquire)) return true;
    using namespace bl::il2cpp;
    if (!api().field_get_parent || !api().runtime_class_init) {
        BL_ERROR("csharp prefix: API de inicializacao das tabelas indisponivel");
        return false;
    }
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    Il2CppClass* lang = findClass({"Terraria", "Lang", {}});
    Il2CppClass* prefixId = findClass({"Terraria.ID", "PrefixID", {}});
    Il2CppClass* sets = prefixId ? findNested(prefixId, "Sets") : nullptr;
    if (!item || !player || !lang || !sets) return false;
    Refs r;
    r.type = fieldOffset(item, "type");
    r.damage = fieldOffset(item, "damage");
    r.useAnimation = fieldOffset(item, "useAnimation");
    r.mana = fieldOffset(item, "mana");
    r.stack = fieldOffset(item, "stack");
    r.knockBack = fieldOffset(item, "knockBack");
    r.ammo = fieldOffset(item, "ammo");
    r.accessory = fieldOffset(item, "accessory");
    r.consumable = fieldOffset(item, "consumable");
    r.ranged = fieldOffset(item, "ranged");
    r.melee = fieldOffset(item, "melee");
    r.magic = fieldOffset(item, "magic");
    r.summon = fieldOffset(item, "summon");
    r.prefix = fieldOffset(item, "prefix");
    r.playerDefense = fieldOffset(player, "statDefense");
    r.playerLife = fieldOffset(player, "statLife");
    r.playerLifeMax = fieldOffset(player, "statLifeMax2");
    r.intClass = findClass({"System", "Int32", {}});
    r.names = findField(lang, "prefix");
    r.chances = findField(sets, "ReducedNaturalChance");
    if (r.type < 0 || r.damage < 0 || r.useAnimation < 0 ||
        r.mana < 0 || r.knockBack < 0 ||
        r.ammo < 0 || r.accessory < 0 || r.consumable < 0 ||
        r.prefix < 0 || r.playerDefense < 0 || !r.intClass ||
        !r.names || !r.chances) {
        BL_ERROR("csharp prefix: campos Terraria insuficientes");
        return false;
    }
    const auto* rollable = findMethodBySignature(
        item, parseSignature("int[] GetRollablePrefixes()"));
    const auto* canRoll = findMethodBySignature(
        item, parseSignature("bool CanRollPrefix(int prefix)"));
    const auto* stats = findMethodBySignature(
        item, parseSignature("bool TryGetPrefixStatMultipliersForItem(int rolledPrefix, out float dmg, out float kb, out float spd, out float size, out float shtspd, out float mcst, out int crt, out int tagdmg, out int arpen, out float value)"));
    const auto* grant = findMethodBySignature(
        player, parseSignature("void GrantPrefixBenefits(Item item)"));
    const auto* rolling = findMethodBySignature(
        item, parseSignature("bool RollAPrefix(UnifiedRandom random, ref int rolledPrefix)"));
    Il2CppClass* randomType = findClass({"Terraria.Utilities", "UnifiedRandom", {}});
    r.nextDoubleMethod = randomType
        ? findMethodBySignature(randomType, parseSignature("double NextDouble()"))
        : nullptr;
    if (!r.nextDoubleMethod) {
        randomType = findClass({"System", "Random", {}});
        r.nextDoubleMethod = randomType
            ? findMethodBySignature(randomType, parseSignature("double NextDouble()"))
            : nullptr;
    }
    r.rollableMethod = rollable;
    if (!rollable || !canRoll || !stats || !grant || !rolling || !r.nextDoubleMethod) {
        BL_ERROR("csharp prefix: assinaturas de reforja ou equipamento ausentes");
        return false;
    }
    g_refs = r;
    if (!bl::hook::install(rollable, hookRollable, &g_originalRollable) ||
        !bl::hook::install(rolling, hookRoll, &g_originalRoll) ||
        !bl::hook::install(canRoll, hookCanRoll, &g_originalCanRoll) ||
        !bl::hook::install(stats, hookStats, &g_originalStats) ||
        !bl::hook::install(grant, hookGrant, &g_originalGrant)) {
        BL_ERROR("csharp prefix: instalacao de hook nativo incompleta");
        return false;
    }
    g_hooksReady.store(true, std::memory_order_release);
    const bool reforgeTooltip = installReforgeTooltipHook();
    const bool inventoryTooltip = installInventoryTooltipHook();
    BL_INFO("csharp prefix: hooks nativos GetRollablePrefixes/RollAPrefix, stats e acessorios ativos; reforgeTooltip=%d inventoryTooltip=%d",
            reforgeTooltip ? 1 : 0, inventoryTooltip ? 1 : 0);
    return true;
}

}

