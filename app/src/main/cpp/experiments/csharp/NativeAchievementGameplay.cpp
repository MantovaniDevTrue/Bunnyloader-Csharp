#include "experiments/csharp/NativeAchievementGameplay.h"
#include "experiments/csharp/NativeAchievementMailbox.h"
#include "experiments/csharp/NativeAchievementManifest.h"
#include "experiments/csharp/NativeAchievementState.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "hook/HookManager.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace bl::csharp {
namespace {

using namespace achievement;
using NpcFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PickupFn = void (*)(Il2CppObject*, Il2CppObject*, int32_t, const MethodInfo*);
using CraftFn = void (*)(Il2CppObject*, const MethodInfo*);
using TileFn = void (*)(Il2CppObject*, uint16_t, const MethodInfo*);

Mailbox g_mailbox;
std::map<std::string, std::pair<std::string, std::vector<Descriptor>>> g_registeredMetadata;
std::map<std::string, std::vector<AchievementState>> g_liveStates;
std::set<std::string> g_pendingState;
NpcFn g_npcOriginal = nullptr;
PickupFn g_pickupOriginal = nullptr;
CraftFn g_craftOriginal = nullptr;
TileFn g_tileOriginal = nullptr;
bool g_npcHook = false;
bool g_pickupHook = false;
bool g_craftHook = false;
bool g_tileHook = false;
unsigned g_ticks = 0;
std::atomic<bool> g_achievementWorldReady{false};

bool typeName(const Il2CppType* type, const char* expected) {
    auto& api = il2cpp::api();
    if (!type || !api.type_get_name || !api.il2cpp_free) return false;
    char* actual = api.type_get_name(type);
    const bool valid = actual && std::strcmp(actual, expected) == 0;
    if (actual) api.il2cpp_free(actual);
    return valid;
}

const MethodInfo* exact(Il2CppClass* cls, const char* name,
                        std::initializer_list<const char*> parameters) {
    auto& api = il2cpp::api();
    if (!cls || !api.class_get_method_from_name || !api.method_get_param ||
        !api.method_get_param_count || !api.method_get_return_type ||
        !api.method_is_instance) return nullptr;
    const auto* method = api.class_get_method_from_name(
        cls, name, static_cast<int>(parameters.size()));
    if (!method || api.method_is_instance(method) ||
        !typeName(api.method_get_return_type(method), "System.Void") ||
        api.method_get_param_count(method) != parameters.size())
        return nullptr;
    uint32_t slot = 0;
    for (const char* expected : parameters)
        if (!typeName(api.method_get_param(method, slot++), expected))
            return nullptr;
    return method;
}

bool intField(Il2CppObject* instance, const char* name, int32_t* result) {
    if (!instance || !result) return false;
    auto& a = il2cpp::api();
    if (!a.object_get_class || !a.class_get_field_from_name ||
        !a.field_get_type || !a.field_get_offset) return false;
    Il2CppClass* cls = a.object_get_class(instance);
    FieldInfo* field = cls ? a.class_get_field_from_name(cls, name) : nullptr;
    if (!field || !typeName(a.field_get_type(field), "System.Int32"))
        return false;
    size_t offset = a.field_get_offset(field);
    if (offset < sizeof(void*) * 2 || offset > 16384) return false;
    std::memcpy(result, reinterpret_cast<const char*>(instance) + offset, sizeof(*result));
    return true;
}

bool currentPlayer(Il2CppObject* player) {
    if (!player) return false;
    auto& api = il2cpp::api();
    if (!api.class_get_field_from_name || !api.field_static_get_value ||
        !api.field_get_type || !api.field_get_flags) return false;
    Il2CppClass* main = il2cpp::findClassQuiet("Terraria", "Main");
    FieldInfo* active = main ? api.class_get_field_from_name(main, "myPlayer") : nullptr;
    if (!active || !(api.field_get_flags(active) & 0x10u) ||
        !typeName(api.field_get_type(active), "System.Int32")) return false;
    int32_t activeIndex = -1;
    api.field_static_get_value(active, &activeIndex);
    int32_t playerIndex = -1;
    return activeIndex >= 0 && activeIndex < 256 &&
           intField(player, "whoAmI", &playerIndex) && playerIndex == activeIndex;
}

bool itemFromObject(Il2CppObject* item, int* type, int* stack) {
    int32_t id = 0, count = 0;
    if (!intField(item, "type", &id) || !intField(item, "stack", &count) ||
        id < 0 || id > 32767 || count < 1 || count > 999999) return false;
    *type = id; *stack = count;
    return true;
}

Il2CppObject* recipeResult(Il2CppObject* recipe) {
    auto& a = il2cpp::api();
    if (!recipe || !a.object_get_class || !a.class_get_field_from_name ||
        !a.field_get_offset || !a.field_get_type) return nullptr;
    Il2CppClass* cls = a.object_get_class(recipe);
    FieldInfo* field = cls ? a.class_get_field_from_name(cls, "createItem") : nullptr;
    if (!field || !typeName(a.field_get_type(field), "Terraria.Item")) return nullptr;
    size_t offset = a.field_get_offset(field);
    if (offset < sizeof(void*) * 2 || offset > 16384) return nullptr;
    Il2CppObject* item = nullptr;
    std::memcpy(&item, reinterpret_cast<const char*>(recipe) + offset, sizeof(item));
    return item;
}

void onNpc(Il2CppObject* player, int32_t type, const MethodInfo* method) {
    if (g_npcOriginal) g_npcOriginal(player, type, method);
    if (g_achievementWorldReady.load(std::memory_order_acquire) && currentPlayer(player))
        g_mailbox.publish({0, type, 1, true});
}

void onPickup(Il2CppObject* player, Il2CppObject* item,
              int32_t customStack, const MethodInfo* method) {
    if (g_pickupOriginal) g_pickupOriginal(player, item, customStack, method);
    int type = 0, stack = 0;
    if (g_achievementWorldReady.load(std::memory_order_acquire) &&
        currentPlayer(player) && itemFromObject(item, &type, &stack))
        g_mailbox.publish({1, type, std::clamp(customStack, 1, 999999), true});
}

void onCraft(Il2CppObject* recipe, const MethodInfo* method) {
    if (g_craftOriginal) g_craftOriginal(recipe, method);
    int type = 0, stack = 0;
    if (g_achievementWorldReady.load(std::memory_order_acquire) &&
        itemFromObject(recipeResult(recipe), &type, &stack))
        g_mailbox.publish({2, type, stack, true});
}

void onTile(Il2CppObject* player, uint16_t tile, const MethodInfo* method) {
    if (g_tileOriginal) g_tileOriginal(player, tile, method);
    if (g_achievementWorldReady.load(std::memory_order_acquire) && currentPlayer(player))
        g_mailbox.publish({3, static_cast<int>(tile), 1, true});
}

void ensureHooks() {
    if (!g_mailbox.anyReady() ||
        (g_npcHook && g_pickupHook && g_craftHook && g_tileHook))
        return;
    auto& api = il2cpp::api();
    if (!api.gameImage || !api.class_get_method_from_name)
        return;
    Il2CppClass* helper = il2cpp::findClassQuiet(
        "Terraria.GameContent.Achievements", "AchievementsHelper");
    if (!helper) return;

    if (!g_npcHook) {
        const MethodInfo* method = exact(helper, "NotifyNPCKilledDirect",
                                         {"Terraria.Player", "System.Int32"});
        if (method) g_npcHook = hook::install(method, &onNpc, &g_npcOriginal);
    }
    if (!g_pickupHook) {
        const MethodInfo* method = exact(helper, "NotifyItemPickup",
                                         {"Terraria.Player", "Terraria.Item", "System.Int32"});
        if (method) g_pickupHook = hook::install(method, &onPickup, &g_pickupOriginal);
    }
    if (!g_craftHook) {
        const MethodInfo* method = exact(helper, "NotifyItemCraft",
                                         {"Terraria.Recipe"});
        if (method) g_craftHook = hook::install(method, &onCraft, &g_craftOriginal);
    }
    if (!g_tileHook) {
        const MethodInfo* method = exact(helper, "NotifyTileDestroyed",
                                         {"Terraria.Player", "System.UInt16"});
        if (method) g_tileHook = hook::install(method, &onTile, &g_tileOriginal);
    }
}

}

bool bindNativeAchievementAssembly(const std::string& mod, const std::string& assembly) {
    if (!g_mailbox.bind(mod, assembly)) return false;
    auto it = g_registeredMetadata.find(mod);
    if (it != g_registeredMetadata.end() && it->second.first != assembly) {
        g_registeredMetadata.erase(it);
        g_liveStates.erase(mod);
        g_pendingState.erase(mod);
    }
    return true;
}

void retireNativeAchievementMod(const std::string& mod) {
    g_mailbox.retire(mod);
    g_registeredMetadata.erase(mod);
    g_liveStates.erase(mod);
    g_pendingState.erase(mod);
}

void clearNativeAchievementWorld() {
    g_achievementWorldReady.store(false, std::memory_order_release);
    g_mailbox.newWorld();
}

void setNativeAchievementWorldState(bool ready) {
    const bool previous = g_achievementWorldReady.exchange(ready, std::memory_order_acq_rel);
    if (previous && !ready) g_mailbox.newWorld();
}

void pumpNativeAchievementGameplayOnGameThread() {
    if (!managedRecipeRuntimeReady()) return;
    if (++g_ticks % 30u == 0u) {
        for (const Candidate& candidate : g_mailbox.pending(1)) {
            std::string result;
            if (!invokeManagedRecipe(candidate.mod, candidate.assembly,
                                      "__achievements__", 0, 0, "", &result, 2500))
                continue;
            if (!result.starts_with("READY|")) continue;
            int count = -1;
            const char* begin = result.data() + 6;
            const char* end = result.data() + result.size();
            const auto parsed = std::from_chars(begin, end, count);
            if (parsed.ec != std::errc{} || parsed.ptr != end) count = -1;
            if (count == 0) {
                g_mailbox.activate(candidate, count);
                continue;
            }
            if (count <= 0 || count > 128) continue;
            std::string definitions;
            if (!invokeManagedRecipe(candidate.mod, candidate.assembly,
                                     "__achievements__", 0, 5, "", &definitions, 2500))
                continue;
            std::vector<Descriptor> decoded;
            if (!parseManifest(candidate.mod, definitions, &decoded) ||
                decoded.size() != static_cast<size_t>(count) ||
                !g_mailbox.activate(candidate, count)) continue;
            g_registeredMetadata[candidate.mod] =
                std::make_pair(candidate.assembly, std::move(decoded));
            g_pendingState.insert(candidate.mod);
        }
        ensureHooks();
    }

    if (g_ticks % 12u == 0u && !g_pendingState.empty()) {
        const std::string mod = *g_pendingState.begin();
        g_pendingState.erase(g_pendingState.begin());
        const auto active = g_registeredMetadata.find(mod);
        if (active != g_registeredMetadata.end()) {
            std::string wire;
            if (invokeManagedRecipe(mod, active->second.first,
                                    "__achievements__", 0, 6, "", &wire, 2500)) {
                std::vector<AchievementState> current;
                if (parseState(mod, wire, active->second.second, &current))
                    g_liveStates[mod] = std::move(current);
                else if (wire.starts_with("PENDING|"))
                    g_pendingState.insert(mod);
            } else {
                g_pendingState.insert(mod);
            }
        }
    }
    if (!g_achievementWorldReady.load(std::memory_order_acquire)) return;
    for (Delivery& event : g_mailbox.drain(2)) {
        std::string result;
        const std::string snapshot = std::to_string(event.event.kind) + "," +
            std::to_string(event.event.type) + "," +
            std::to_string(event.event.stack) + "," +
            (event.event.local ? "1" : "0");
        if (!invokeManagedRecipe(event.mod, event.assembly, "__achievements__",
                                 0, 1, snapshot, &result, 1500) ||
            result.starts_with("PENDING|")) {
            g_mailbox.retry(std::move(event));
        } else if (result.starts_with("EVENT|")) {
            g_pendingState.insert(event.mod);
        }
    }
}

}
