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
}

}
