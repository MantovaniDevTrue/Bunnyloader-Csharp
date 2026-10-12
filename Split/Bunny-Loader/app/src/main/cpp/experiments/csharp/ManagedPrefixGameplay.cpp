#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
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
#include <atomic>
#include <mutex>
#include <cmath>
#include <cstdint>
#include <cstring>
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

struct Refs {
    int type = -1;
    int damage = -1;
    int useAnimation = -1;
    int mana = -1;
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
    Il2CppClass* intClass = nullptr;
    FieldInfo* names = nullptr;
    FieldInfo* chances = nullptr;
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
std::atomic<int> g_reforgeDiagnostics{0};

Il2CppArray* staticArray(FieldInfo* field) {
    Il2CppArray* value = nullptr;
    if (field) bl::il2cpp::api().field_static_get_value(field, &value);
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

bool eligible(Il2CppObject* item, const ManagedPrefixEntry& p, bool vanillaHasRollable) {
    const auto& r = g_refs;
    if (!item) return false;
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

Il2CppArray* hookRollable(Il2CppObject* item, const MethodInfo* method) {
    Il2CppArray* base = g_originalRollable ? g_originalRollable(item, method) : nullptr;
    if (!g_hooksReady || !item) return base;
    const auto descriptors = snapshotManagedPrefixes();
    if (descriptors.empty() || !ensurePrefixTables(descriptors)) return base;
    const bool vanillaHasRollable = base && base->length > 0;
    int extra = 0;
    for (const auto& p : descriptors)
        if (eligible(item, p, vanillaHasRollable))
            extra += static_cast<int>(p.rollWeight);
    if (extra == 0) return base;
    const uintptr_t old = base ? base->length : 0;
    const uintptr_t total = old + static_cast<uintptr_t>(extra);
    if (total > 512 || !g_refs.intClass) return base;
    Il2CppArray* out = bl::il2cpp::api().array_new(g_refs.intClass, total);
    if (!out) return base;
    auto* values = static_cast<int32_t*>(arrayData(out));
    if (old) std::memcpy(values, arrayData(base), old * sizeof(int32_t));
    uintptr_t index = old;
    for (const auto& p : descriptors)
        if (eligible(item, p, vanillaHasRollable))
            for (int i = 0; i < static_cast<int>(p.rollWeight); ++i)
                values[index++] = p.type;
    return out;
}


bool hookRoll(Il2CppObject* item, Il2CppObject* random, int32_t* rolled,
              const MethodInfo* method) {
    if (!g_hooksReady.load(std::memory_order_acquire) ||
        !item || !random || !rolled || !g_refs.rollableMethod ||
        !g_refs.nextDoubleMethod)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;

    const auto descriptors = snapshotManagedPrefixes();
    if (descriptors.empty())
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;

    auto& api = bl::il2cpp::api();
    Il2CppObject* error = nullptr;
    auto* list = reinterpret_cast<Il2CppArray*>(
        api.runtime_invoke(g_refs.rollableMethod, item, nullptr, &error));
    if (error || !list || list->length == 0 || list->length > 512)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;

    const auto* values = static_cast<const int32_t*>(arrayData(list));
    bool hasManaged = false;
    bool hasOtherMod = false;
    const int vanillaCount = bl::runtime::vanillaPrefixCount();
    for (uintptr_t i = 0; i < list->length; ++i) {
        if (byType(descriptors, values[i])) hasManaged = true;
        else if (values[i] >= vanillaCount &&
                 !bl::runtime::modPrefixKey(values[i]).empty()) {
            hasOtherMod = true;
            break;
        }
    }
    if (!hasManaged || hasOtherMod)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;

    error = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(
        g_refs.nextDoubleMethod, random, nullptr, &error);
    if (error || !boxed)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;
    double chance = 0.0;
    std::memcpy(&chance,
                reinterpret_cast<const char*>(boxed) + sizeof(Il2CppObject),
                sizeof(chance));
    if (!std::isfinite(chance) || chance < 0.0 || chance >= 1.0)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;

    uintptr_t index = static_cast<uintptr_t>(chance * static_cast<double>(list->length));
    if (index >= list->length) index = list->length - 1;
    *rolled = values[index];
    const int reported = g_reforgeDiagnostics.fetch_add(1, std::memory_order_relaxed);
    if (reported < 8)
        BL_INFO("csharp prefix: RollAPrefix selecionou %d de %zu candidatos (%d gerenciados)",
                *rolled, static_cast<size_t>(list->length),
                static_cast<int>(descriptors.size()));
    return true;
}

bool hookCanRoll(Il2CppObject* item, int32_t prefix, const MethodInfo* method) {
    if (g_hooksReady) {
        const auto list = snapshotManagedPrefixes();
        if (const auto* p = byType(list, prefix))
            return eligible(item, *p, true) && ensurePrefixTables(list);
    }
    return g_originalCanRoll ? g_originalCanRoll(item, prefix, method) : false;
}

bool hookStats(Il2CppObject* item, int32_t prefix, float* dmg, float* kb,
               float* spd, float* size, float* shootSpeed, float* mana,
               int32_t* crit, int32_t* tagDamage, int32_t* pen,
               float* value, const MethodInfo* method) {
    if (g_hooksReady && item) {
        const auto list = snapshotManagedPrefixes();
        if (const auto* p = byType(list, prefix)) {
            if (!dmg || !kb || !spd || !size || !shootSpeed || !mana ||
                !crit || !tagDamage || !pen || !value ||
                !ensurePrefixTables(list) || !eligible(item, *p, true))
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
            return changes(itemDamage, *dmg) && changes(itemAnimation, *spd) &&
                   changes(itemMana, *mana) && (*kb == 1.f || itemKnockBack != 0.f);
        }
    }
    return g_originalStats ? g_originalStats(item, prefix, dmg, kb, spd, size,
                                            shootSpeed, mana, crit, tagDamage,
                                            pen, value, method) : false;
}

void hookGrant(Il2CppObject* player, Il2CppObject* item, const MethodInfo* method) {
    if (g_originalGrant) g_originalGrant(player, item, method);
    if (!g_hooksReady || !player || !item) return;
    const int prefix = bl::runtime::field<uint8_t>(item, g_refs.prefix);
    const auto list = snapshotManagedPrefixes();
    const auto* found = byType(list, prefix);
    if (!found || found->category != 5 || !eligible(item, *found, true)) return;
    bl::runtime::field<int32_t>(player, g_refs.playerDefense) += found->accessoryDefense;
}

}

bool installManagedPrefixGameplayHooks() {
    std::lock_guard<std::mutex> guard(g_installMutex);
    if (g_hooksReady.load(std::memory_order_acquire)) return true;
    using namespace bl::il2cpp;
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
    BL_INFO("csharp prefix: hooks nativos GetRollablePrefixes/RollAPrefix, stats e acessorios ativos");
    return true;
}

}
