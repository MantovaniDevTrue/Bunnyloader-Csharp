#include "experiments/csharp/NativeRarityGameplay.h"
#include "experiments/csharp/NativeRarityPalette.h"
#include "experiments/csharp/NativeRarityColorWire.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "content/common/GameRefs.h"
#include <cmath>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace bl::csharp {
namespace {

struct RaritySample {
    int type = -1;
    std::string name;
};

struct RarityMod {
    std::string id;
    std::string assembly;
    std::vector<RaritySample> rarities;
    size_t cursor = 0;
    bool ready = false;
    bool failed = false;
};

std::mutex g_mutex;
std::vector<RarityMod> g_mods;
NativeRarityPalette g_palette;
NativeColorLayout g_colorLayout;
std::atomic<bool> g_hookReady{false};
unsigned g_tick = 0;
size_t g_modCursor = 0;
using PopupColorFn = uint32_t (*)(int32_t, const MethodInfo*);
PopupColorFn g_originalPopup = nullptr;

using PrefixFn = bool (*)(Il2CppObject*, int32_t, bool*, const MethodInfo*);
using PrefixStatsFn = bool (*)(Il2CppObject*, int32_t, float*, float*, float*,
    float*, float*, float*, int32_t*, int32_t*, int32_t*, float*, const MethodInfo*);
PrefixFn g_originalPrefix = nullptr;
const MethodInfo* g_prefixStats = nullptr;
int g_rareOffset = -1;
int g_prefixOffset = -1;
std::atomic<bool> g_prefixReady{false};

float currentPrefixMultiplier(Il2CppObject* item) {
    if (!item || !g_prefixStats || g_prefixOffset < 0) return 1.f;
    const auto prefix = static_cast<int32_t>(bl::runtime::field<uint8_t>(item, g_prefixOffset));
    if (prefix == 0) return 1.f;
    auto* pointer = bl::il2cpp::methodPointer(g_prefixStats);
    if (!pointer) return 1.f;
    float dmg = 1.f, kb = 1.f, speed = 1.f, size = 1.f, shot = 1.f, mana = 1.f, value = 1.f;
    int32_t crit = 0, tag = 0, pen = 0;
    auto fn = reinterpret_cast<PrefixStatsFn>(pointer);
    if (!fn(item, prefix, &dmg, &kb, &speed, &size, &shot, &mana,
            &crit, &tag, &pen, &value, g_prefixStats) ||
        !std::isfinite(value) || value < 0.f || value > 100.f) return 1.f;
    return value;
}

bool prefixRarity(Il2CppObject* item, int32_t requested, bool* topTier, const MethodInfo* method) {
    if (!g_originalPrefix) return false;
    if (!item || !g_prefixReady.load(std::memory_order_acquire) ||
        g_rareOffset < 0 || requested == -3)
        return g_originalPrefix(item, requested, topTier, method);
    const int32_t base = bl::runtime::field<int32_t>(item, g_rareOffset);
    uint32_t cached = 0;
    if (!g_palette.read(base, &cached))
        return g_originalPrefix(item, requested, topTier, method);
    const bool succeeded = g_originalPrefix(item, requested, topTier, method);
    if (!succeeded) return false;

    const auto rarities = nativeRarities().snapshot();
    const auto found = std::find_if(rarities->begin(), rarities->end(),
        [base](const NativeRarityIdentity& identity) { return identity.type == base; });
    if (found == rarities->end()) return true;

    RarityMod owner;
    {
        std::unique_lock lock(g_mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            for (const auto& mod : g_mods)
                if (mod.id == found->mod && mod.ready && !mod.failed) {
                    owner.id = mod.id;
                    owner.assembly = mod.assembly;
                    break;
                }
        }
    }
    const int32_t vanilla = bl::runtime::field<int32_t>(item, g_rareOffset);
    if (owner.id.empty()) {
        bl::runtime::field<int32_t>(item, g_rareOffset) = base;
        return true;
    }

    const float value = currentPrefixMultiplier(item);
    char number[48]{};
    const auto encoded = std::to_chars(number, number + sizeof(number), value);
    if (encoded.ec != std::errc{})
        bl::runtime::field<int32_t>(item, g_rareOffset) = base;
    else {
        std::string response;
        const std::string payload(number, encoded.ptr);
        const bool called = invokeManagedRarity(owner.id, owner.assembly, found->name,
            1, payload + "," + std::to_string(vanilla), &response, 1800);
        int resultingType = base;
        if (called && decodeRarityPrefixWire(response, &resultingType))
            bl::runtime::field<int32_t>(item, g_rareOffset) = resultingType;
        else
            bl::runtime::field<int32_t>(item, g_rareOffset) = base;
    }
    return true;
}

bool installPrefixRarityHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_prefixReady.load(std::memory_order_acquire)) return true;
    auto& a = api();
    auto* item = findClass({"Terraria", "Item", {}});
    const auto* prefix = item ? findMethodBySignature(item,
        parseSignature("bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)")) : nullptr;
    const auto* stats = item ? findMethodBySignature(item,
        parseSignature("bool TryGetPrefixStatMultipliersForItem(int rolledPrefix, out float dmg, out float kb, out float spd, out float size, out float shtspd, out float mcst, out int crt, out int tagdmg, out int arpen, out float value)")) : nullptr;
    if (!prefix || !stats || !a.method_is_instance || !a.method_is_instance(prefix) ||
        !a.method_is_instance(stats) || !methodPointer(stats)) {
        *error = "Reason=PrefixMethodSignatureUnavailable";
        return false;
    }
    const auto rareField = findField(item, "rare");
    const auto prefixField = findField(item, "prefix");
    auto validField = [&](FieldInfo* field, const char* typeName) {
        if (!field || !a.field_get_type || !a.field_get_flags || !a.class_from_il2cpp_type ||
            !a.class_get_namespace || !a.class_get_name ||
            (a.field_get_flags(field) & 0x10u)) return false;
        auto* type = a.class_from_il2cpp_type(a.field_get_type(field));
        const char* ns = type ? a.class_get_namespace(type) : nullptr;
        const char* name = type ? a.class_get_name(type) : nullptr;
        return ns && name && !std::strcmp(ns, "System") && !std::strcmp(name, typeName);
    };
    if (!validField(rareField, "Int32") || !validField(prefixField, "Byte")) {
        *error = "Reason=ItemRarityOrPrefixFieldAbiMismatch";
        return false;
    }
    const int rarityOffset = fieldOffset(item, "rare");
    const int prefixOffset = fieldOffset(item, "prefix");
    if (rarityOffset < 0 || prefixOffset < 0) {
        *error = "Reason=ItemRarityFieldOffsetsUnavailable";
        return false;
    }
    g_rareOffset = rarityOffset;
    g_prefixOffset = prefixOffset;
    g_prefixStats = stats;
    if (!bl::hook::install(prefix, prefixRarity, &g_originalPrefix)) {
        *error = "Reason=ItemPrefixHookRejected";
        return false;
    }
    g_prefixReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Item.Prefix Result=Ready");
    return true;
}

uint32_t popupRarityColor(int32_t type, const MethodInfo* method) {
    uint32_t argb = 0;
    if (g_hookReady.load(std::memory_order_acquire) && g_palette.read(type, &argb))
        return g_colorLayout.fromArgb(argb);
    return g_originalPopup ? g_originalPopup(type, method) : 0xffffffffu;
}

bool installPopupRarityHook(std::string* error) {
    using namespace bl::il2cpp;
    if (g_hookReady.load(std::memory_order_acquire)) return true;
    auto& a = api();
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    const MethodInfo* method = item ? findMethodBySignature(
        item, parseSignature("Color GetPopupRarityColor(int itemRarity)")) : nullptr;
    if (!method || !a.method_is_instance || a.method_is_instance(method) ||
        !a.method_get_return_type || !a.method_get_param_count ||
        !a.method_get_param || a.method_get_param_count(method) != 1 ||
        !a.class_from_il2cpp_type || !a.class_get_namespace || !a.class_get_name) {
        *error = "Reason=PopupMethodSignatureUnavailable";
        return false;
    }
    auto* argument = a.class_from_il2cpp_type(a.method_get_param(method, 0));
    const char* argumentNamespace = argument ? a.class_get_namespace(argument) : nullptr;
    const char* argumentName = argument ? a.class_get_name(argument) : nullptr;
    if (!argumentNamespace || !argumentName ||
        std::strcmp(argumentNamespace, "System") || std::strcmp(argumentName, "Int32")) {
        *error = "Reason=PopupRarityArgumentAbiMismatch";
        return false;
    }
    auto* color = a.class_from_il2cpp_type(a.method_get_return_type(method));
    if (!g_colorLayout.bind(color, error)) return false;
    if (!bl::hook::install(method, popupRarityColor, &g_originalPopup)) {
        *error = "Reason=PopupColorHookRejected";
        return false;
    }
    g_hookReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][RarityLoader] Stage=InstallHook API=Item.GetPopupRarityColor Result=Ready");
    return true;
}

struct DiscoField {
    FieldInfo* field = nullptr;
    int bytes = 0;
};

DiscoField discoField(Il2CppClass* main, const char* name) {
    using namespace bl::il2cpp;
    auto& a = api();
    FieldInfo* field = main ? findField(main, name) : nullptr;
    if (!field || !a.field_get_flags || !(a.field_get_flags(field) & 0x10u) ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_namespace || !a.class_get_name) return {};
    auto* type = a.class_from_il2cpp_type(a.field_get_type(field));
    const char* ns = type ? a.class_get_namespace(type) : nullptr;
    const char* kind = type ? a.class_get_name(type) : nullptr;
    if (!ns || !kind || std::strcmp(ns, "System")) return {};
    if (!std::strcmp(kind, "Byte")) return {field, 1};
    if (!std::strcmp(kind, "Int32")) return {field, 4};
    return {};
}

std::string discoSnapshot() {
    using namespace bl::il2cpp;
    static bool bound = false;
    static DiscoField fields[3]{};
    if (!bound) {
        Il2CppClass* main = findClass({"Terraria", "Main", {}});
        fields[0] = discoField(main, "DiscoR");
        fields[1] = discoField(main, "DiscoG");
        fields[2] = discoField(main, "DiscoB");
        bound = fields[0].field && fields[1].field && fields[2].field;
    }
    if (!bound || !api().field_static_get_value) return "255,255,255";
    int channels[3]{};
    for (int i = 0; i < 3; ++i) {
        if (fields[i].bytes == 1) {
            uint8_t value = 255;
            api().field_static_get_value(fields[i].field, &value);
            channels[i] = value;
        } else {
            int32_t value = 255;
            api().field_static_get_value(fields[i].field, &value);
            channels[i] = std::clamp(value, 0, 255);
        }
    }
    return std::to_string(channels[0]) + "," +
        std::to_string(channels[1]) + "," + std::to_string(channels[2]);
}

}

bool registerNativeRaritySampling(const std::string& mod, const std::string& assembly) {
    if (mod.empty() || assembly.empty()) return false;
    RarityMod entry;
    entry.id = mod;
    entry.assembly = assembly;
    const auto snapshot = nativeRarities().snapshot();
    for (const auto& rarity : *snapshot)
        if (rarity.mod == mod) entry.rarities.push_back({rarity.type, rarity.name});
    if (entry.rarities.empty()) return false;
    std::lock_guard guard(g_mutex);
    for (const auto& previous : g_mods)
        if (previous.id == mod) return false;
    g_mods.push_back(std::move(entry));
    return true;
}

void pumpNativeRarityGameplay() {
    ++g_tick;
    if ((g_tick & 1u) != 0) return;
    RarityMod current;
    {
        std::lock_guard guard(g_mutex);
        if (g_mods.empty()) return;
        bool found = false;
        for (size_t i = 0; i < g_mods.size(); ++i) {
            g_modCursor = (g_modCursor + 1) % g_mods.size();
            auto& mod = g_mods[g_modCursor];
            if (mod.failed) continue;
            current.id = mod.id;
            current.assembly = mod.assembly;
            if (mod.ready && !mod.rarities.empty()) {
                current.rarities.push_back(mod.rarities[mod.cursor % mod.rarities.size()]);
                mod.cursor = (mod.cursor + 1) % mod.rarities.size();
            }
            found = true;
            break;
        }
        if (!found) return;
    }

    if (current.rarities.empty()) {
        std::string error;
        const int state = managedRarityWarmupState(current.id, current.assembly, &error);
        if (state == 0) return;
        std::lock_guard guard(g_mutex);
        for (auto& mod : g_mods) {
            if (mod.id != current.id) continue;
            mod.ready = state == 1;
            mod.failed = state < 0;
            if (mod.failed)
                BL_ERROR("[CSharp][RarityLoader] Mod=%s Stage=Warmup Result=Rejected Detail=%s",
                    mod.id.c_str(), error.c_str());
            break;
        }
        return;
    }

    std::string response;
    const auto& sample = current.rarities.front();
    if (!invokeManagedRarity(current.id, current.assembly, sample.name,
            0, discoSnapshot(), &response, 1200)) return;
    uint32_t argb = 0;
    if (!decodeRarityColorWire(response, &argb) || !g_palette.publish(sample.type, argb)) return;
    if (!g_hookReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installPopupRarityHook(&error))
            BL_WARN("[CSharp][RarityLoader] Stage=InstallPopup Result=Pending Detail=%s",
                error.c_str());
    }
    if (g_hookReady.load(std::memory_order_acquire) &&
        !g_prefixReady.load(std::memory_order_acquire) && (g_tick % 60u == 0)) {
        std::string error;
        if (!installPrefixRarityHook(&error))
            BL_WARN("[CSharp][RarityLoader] Stage=InstallPrefix Result=Pending Detail=%s",
                error.c_str());
    }
}

}
