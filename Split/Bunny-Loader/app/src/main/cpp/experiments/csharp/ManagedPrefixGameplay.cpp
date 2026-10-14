#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
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
#include <atomic>
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
ReforgeFn g_originalReforge = nullptr;
InventoryTooltipFn g_originalInventoryTooltip = nullptr;
std::atomic<bool> g_inventoryTooltipReady{false};
std::atomic<bool> g_reforgeTooltipReady{false};
std::atomic<int> g_reforgeDiagnostics{0};
struct RestoreRoll {
    Il2CppObject* item = nullptr;
    int prefix = 0;
    bool used = false;
};
thread_local RestoreRoll g_restoreRoll{};

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
    if (g_restoreRoll.item == item && g_restoreRoll.prefix > 0) {
        if (const auto* target = byType(descriptors, g_restoreRoll.prefix)) {
            if (eligible(item, *target, true) && ensurePrefixTables(descriptors)) {
                *rolled = target->type;
                g_restoreRoll.used = true;
                return true;
            }
        }
    }
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

    const int chosen = weightedPrefixCandidate(
        values, static_cast<size_t>(list->length), chance);
    if (chosen < 0)
        return g_originalRoll ? g_originalRoll(item, random, rolled, method) : false;
    *rolled = chosen;
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
    const auto prefixes = snapshotManagedPrefixes();
    const auto* p = byType(prefixes, prefix);
    if (!p || p->category != 5 || p->accessoryDefense == 0 ||
        !eligible(item, *p, true)) return;
    auto* text = bl::runtime::field<Il2CppArray*>(popup, r.reforgeText);
    auto* bad = bl::runtime::field<Il2CppArray*>(popup, r.reforgeBad);
    if (!text || !bad || text->length < 1 || bad->length < 1) return;
    const int count = bl::runtime::field<int32_t>(popup, r.reforgeNumLines);
    if (count < 0 || count > 1) return;
    const int bonus = p->accessoryDefense;
    const std::string description = (bonus > 0 ? "+" : "") + std::to_string(bonus) +
                                    accessoryDefenseSuffix();
    auto& api = bl::il2cpp::api();
    if (!api.string_new || !api.gc_wbarrier_set_field) return;
    Il2CppString* value = api.string_new(description.c_str());
    if (!value) return;
    auto** strings = static_cast<Il2CppString**>(arrayData(text));
    api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(text),
                             reinterpret_cast<void**>(&strings[0]), value);
    static_cast<uint8_t*>(arrayData(bad))[0] = bonus < 0 ? 1 : 0;
    bl::runtime::field<int32_t>(popup, r.reforgeNumLines) = 1;
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
    const auto prefixes = snapshotManagedPrefixes();
    const auto* current = byType(prefixes, prefixType);
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
    const auto descriptors = snapshotManagedPrefixes();
    const auto* requested = byType(descriptors, prefix);
    if (!requested || !eligible(item, *requested, true) ||
        !ensurePrefixTables(descriptors)) return false;
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
    const auto descriptors = snapshotManagedPrefixes();
    const auto* policy = byType(descriptors, prefix);
    if (!policy) return reject("PrefixDescriptorMissing");
    if (!eligible(item, *policy, true)) return reject("ItemNotEligible");
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

bool managedPrefixGameplayReady() {
    return g_hooksReady.load(std::memory_order_acquire) &&
           !snapshotManagedPrefixes().empty();
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
    const bool reforgeTooltip = installReforgeTooltipHook();
    const bool inventoryTooltip = installInventoryTooltipHook();
    BL_INFO("csharp prefix: hooks nativos GetRollablePrefixes/RollAPrefix, stats e acessorios ativos; reforgeTooltip=%d inventoryTooltip=%d",
            reforgeTooltip ? 1 : 0, inventoryTooltip ? 1 : 0);
    return true;
}

}

