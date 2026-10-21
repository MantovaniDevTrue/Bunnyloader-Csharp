#include "experiments/csharp/NativeAchievementHooks.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "content/common/GameRefs.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {

using NpcKilledFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PickupFn = void (*)(Il2CppObject*, Il2CppObject*, int32_t, const MethodInfo*);
using CraftFn = void (*)(Il2CppObject*, const MethodInfo*);
using TileDestroyedFn = void (*)(Il2CppObject*, uint16_t, const MethodInfo*);

NpcKilledFn originalNpcKilled = nullptr;
PickupFn originalPickup = nullptr;
CraftFn originalCraft = nullptr;
TileDestroyedFn originalTileDestroyed = nullptr;

std::array<bool, 4> installed{};
std::atomic<bool> hooksReady{false};
std::atomic<bool> worldActive{false};
std::atomic<bool> bindingsReady{false};
std::atomic<uint64_t> metadataRevision{0};
std::atomic<uint64_t> rejectedEvents{0};
std::mutex bindingMutex;
struct AchievementBinding {
    std::string assembly;
    std::vector<AchievementMetadata> metadata;
};
std::map<std::string, AchievementBinding> bindings;
FieldInfo* playersField = nullptr;
FieldInfo* localIndexField = nullptr;
FieldInfo* networkModeField = nullptr;
const MethodInfo* localIndexGetter = nullptr;
Il2CppClass* playerClass = nullptr;
int itemTypeOffset = -1;
int itemStackOffset = -1;
int recipeResultOffset = -1;

bool nativeInt(void* object, int offset, int* value) {
    if (!object || offset < 0 || !value) return false;
    std::memcpy(value, static_cast<const char*>(object) + offset, sizeof(*value));
    return true;
}

int localIndex() {
    int index = -1;
    if (localIndexField) il2cpp::api().field_static_get_value(localIndexField, &index);
    else if (localIndexGetter && il2cpp::methodPointer(localIndexGetter)) {
        auto get = reinterpret_cast<int (*)(const MethodInfo*)>(
            il2cpp::methodPointer(localIndexGetter));
        index = get(localIndexGetter);
    }
    return index;
}

Il2CppObject* localPlayer() {
    if (!playersField || !playerClass) return nullptr;
    Il2CppArray* players = nullptr;
    auto& api = il2cpp::api();
    api.field_static_get_value(playersField, &players);
    const int index = localIndex();
    if (!players || index < 0 || !api.class_get_rank ||
        !api.class_get_element_class ||
        api.class_get_rank(players->obj.klass) != 1 ||
        api.class_get_element_class(players->obj.klass) != playerClass ||
        static_cast<uintptr_t>(index) >= players->length)
        return nullptr;
    return static_cast<Il2CppObject**>(arrayData(players))[index];
}

bool singlePlayerOrClient() {
    if (!networkModeField) return false;
    int networkMode = -1;
    il2cpp::api().field_static_get_value(networkModeField, &networkMode);
    return networkMode == 0 || networkMode == 1;
}

void notify(int operation, int type, int count, bool local) {
    if (!hooksReady.load(std::memory_order_acquire) ||
        !worldActive.load(std::memory_order_acquire) ||
        !singlePlayerOrClient() || !local || type < -32768 ||
        type > 1000000 || count < 1 || count > 9999)
        return;

    // Poucos mods inscritos. A fila MonoVM recebe somente valores primitivos;
    // nenhuma entidade IL2CPP atravessa a fronteira entre threads.
    std::lock_guard<std::mutex> lock(bindingMutex);
    for (const auto& [mod, binding] : bindings) {
        if (!queueManagedAchievementEvent(mod, binding.assembly, operation, type, count, true)) {
            const auto failures = rejectedEvents.fetch_add(1, std::memory_order_relaxed) + 1;
            if (failures <= 4 || failures % 256 == 0)
                BL_WARN("[CSharp][AchievementLoader] Evento nao entregue mod=%s kind=%d type=%d total=%llu",
                        mod.c_str(), operation, type,
                        static_cast<unsigned long long>(failures));
        }
    }
}

void hkNpcKilled(Il2CppObject* player, int32_t npcNetId, const MethodInfo* method) {
    if (originalNpcKilled) originalNpcKilled(player, npcNetId, method);
    if (player && player == localPlayer()) notify(1, npcNetId, 1, true);
}

void hkItemPickup(Il2CppObject* player, Il2CppObject* item,
                  int32_t customStack, const MethodInfo* method) {
    if (originalPickup) originalPickup(player, item, customStack, method);
    if (!item || !player || player != localPlayer()) return;
    int itemType = -1, count = customStack;
    if (!nativeInt(item, itemTypeOffset, &itemType)) return;
    if (count <= 0 && !nativeInt(item, itemStackOffset, &count)) return;
    notify(2, itemType, count, true);
}

void hkItemCraft(Il2CppObject* recipe, const MethodInfo* method) {
    if (originalCraft) originalCraft(recipe, method);
    if (!recipe || !localPlayer()) return;
    Il2CppObject* item = nullptr;
    if (recipeResultOffset < 0) return;
    std::memcpy(&item, reinterpret_cast<const char*>(recipe) + recipeResultOffset,
                sizeof(item));
    if (!item) return;
    int itemType = -1, stack = 0;
    if (!nativeInt(item, itemTypeOffset, &itemType) ||
        !nativeInt(item, itemStackOffset, &stack)) return;
    notify(3, itemType, stack, true);
}

void hkTileDestroyed(Il2CppObject* player, uint16_t tile,
                     const MethodInfo* method) {
    if (originalTileDestroyed) originalTileDestroyed(player, tile, method);
    if (player && player == localPlayer()) notify(4, tile, 1, true);
}

const MethodInfo* checked(Il2CppClass* klass, const char* signature,
                          std::string* error) {
    const auto expected = il2cpp::parseSignature(signature);
    const MethodInfo* method = klass
        ? il2cpp::findMethodBySignature(klass, expected) : nullptr;
    if (!method || !il2cpp::methodPointer(method) ||
        il2cpp::api().method_is_instance(method)) {
        *error = std::string("Achievement signature unavailable: ") + signature;
        return nullptr;
    }
    return method;
}

bool validateFields(std::string* error) {
    auto& api = il2cpp::api();
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    auto* item = il2cpp::findClass({"Terraria", "Item", {}});
    auto* recipe = il2cpp::findClass({"Terraria", "Recipe", {}});
    if (!main || !player || !item || !recipe ||
        !api.field_static_get_value || !api.method_is_instance ||
        !api.class_get_rank || !api.class_get_element_class) {
        *error = "Achievement class/export unavailable";
        return false;
    }

    if (!il2cpp::nativeFieldContract(main, "player", "Terraria.Player[]",
            true, sizeof(void*), &playersField, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "netMode", "System.Int32",
            true, sizeof(int), &networkModeField, nullptr, error) ||
        !il2cpp::nativeFieldContract(item, "type", "System.Int32",
            false, sizeof(int), nullptr, &itemTypeOffset, error) ||
        !il2cpp::nativeFieldContract(item, "stack", "System.Int32",
            false, sizeof(int), nullptr, &itemStackOffset, error) ||
        !il2cpp::nativeFieldContract(recipe, "createItem", "Terraria.Item",
            false, sizeof(void*), nullptr, &recipeResultOffset, error))
        return false;

    std::string localError;
    if (!il2cpp::nativeFieldContract(main, "myPlayer", "System.Int32",
            true, sizeof(int), &localIndexField, nullptr, &localError)) {
        localIndexGetter = checked(main, "int get_myPlayer()", error);
        if (!localIndexGetter) return false;
    }
    playerClass = player;
    return true;
}

}

bool registerNativeAchievementMod(const std::string& modId,
                                  const std::string& assemblyPath,
                                  const std::string& metadata,
                                  std::string* error) {
    if (!error) return false;
    if (assemblyPath.empty() || assemblyPath.size() > 1024) {
        *error = "Invalid achievement assembly path";
        return false;
    }
    std::vector<AchievementMetadata> records;
    if (!parseAchievementMetadata(metadata, modId, &records, error))
        return false;
    std::lock_guard<std::mutex> lock(bindingMutex);
    const auto found = bindings.find(modId);
    if (found != bindings.end()) {
        if (found->second.assembly != assemblyPath) {
            *error = "Achievement mod path changed without unload";
            return false;
        }
        found->second.metadata = std::move(records);
        metadataRevision.fetch_add(1, std::memory_order_release);
        return true;
    }
    if (bindings.size() >= 64) {
        *error = "Achievement mod capacity exceeded";
        return false;
    }
    bindings.emplace(modId, AchievementBinding{assemblyPath, std::move(records)});
    setManagedAchievementMetadataPublisher(&updateNativeAchievementProgress);
    bindingsReady.store(true, std::memory_order_release);
    metadataRevision.fetch_add(1, std::memory_order_release);
    return true;
}

bool updateNativeAchievementProgress(const std::string& modId,
                                     const std::string& snapshot,
                                     std::string* error) {
    if (!error) return false;
    std::vector<AchievementMetadata> entries;
    if (!parseAchievementMetadata(snapshot, modId, &entries, error))
        return false;
    std::lock_guard<std::mutex> lock(bindingMutex);
    auto it = bindings.find(modId);
    if (it == bindings.end() || it->second.metadata.size() != entries.size()) {
        *error = "Achievement identity set changed after native registration";
        return false;
    }

    // Impedir que callbacks modifiquem identidade, categoria ou assets depois
    // de registrados; só a conclusão/progresso/visibilidade são dinâmicos.
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& old = it->second.metadata[i];
        const auto& entry = entries[i];
        if (old.name != entry.name || old.title != entry.title ||
            old.description != entry.description ||
            old.texture != entry.texture || old.index != entry.index ||
            old.category != entry.category || (old.completed && !entry.completed)) {
            *error = "Achievement static metadata changed or progress regressed";
            return false;
        }
    }
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto& old = it->second.metadata[i];
        old.completed = entries[i].completed;
        old.hidden = entries[i].hidden;
        old.progress = entries[i].progress;
    }
    metadataRevision.fetch_add(1, std::memory_order_release);
    return true;
}

std::uint64_t nativeAchievementMetadataRevision() {
    return metadataRevision.load(std::memory_order_acquire);
}

std::vector<AchievementMetadata> nativeAchievementMetadataSnapshot() {
    std::lock_guard<std::mutex> lock(bindingMutex);
    std::vector<AchievementMetadata> all;
    for (const auto& [mod, binding] : bindings)
        all.insert(all.end(), binding.metadata.begin(), binding.metadata.end());
    return all;
}

bool nativeAchievementBindingsReady() {
    return bindingsReady.load(std::memory_order_acquire);
}

bool nativeAchievementHooksReady() {
    return hooksReady.load(std::memory_order_acquire);
}

void flushNativeAchievementProgressOnExit() {
    worldActive.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lock(bindingMutex);
    for (const auto& [mod, binding] : bindings) {
        if (!queueManagedAchievementFlush(mod, binding.assembly))
            BL_WARN("[CSharp][AchievementLoader] Flush enfileirado recusado mod=%s",
                    mod.c_str());
    }
}

void setNativeAchievementWorldActive(bool active) {
    worldActive.store(active, std::memory_order_release);
}

bool installNativeAchievementHooks(std::string* error) {
    if (!error) return false;
    error->clear();
    if (hooksReady.load(std::memory_order_acquire)) return true;
    if (!validateFields(error)) return false;
    auto* helper = il2cpp::findClass({
        "Terraria.GameContent.Achievements", "AchievementsHelper", {}});
    if (!helper) {
        *error = "Terraria AchievementsHelper class unavailable";
        return false;
    }
    const std::array<const char*, 4> signatures{
        "void NotifyNPCKilledDirect(Player player, int npcNetID)",
        "void NotifyItemPickup(Player player, Item item, int customStack)",
        "void NotifyItemCraft(Recipe recipe)",
        "void NotifyTileDestroyed(Player player, ushort tile)"
    };
    std::array<const MethodInfo*, 4> methods{};
    for (std::size_t i = 0; i < methods.size(); ++i) {
        methods[i] = checked(helper, signatures[i], error);
        if (!methods[i]) return false;
    }

    if (!installed[0]) installed[0] =
        hook::install(methods[0], hkNpcKilled, &originalNpcKilled);
    if (!installed[1]) installed[1] =
        hook::install(methods[1], hkItemPickup, &originalPickup);
    if (!installed[2]) installed[2] =
        hook::install(methods[2], hkItemCraft, &originalCraft);
    if (!installed[3]) installed[3] =
        hook::install(methods[3], hkTileDestroyed, &originalTileDestroyed);
    for (std::size_t i = 0; i < installed.size(); ++i) {
        if (!installed[i]) {
            *error = std::string("Achievement hook install failed: ") + signatures[i];
            return false;
        }
    }
    hooksReady.store(true, std::memory_order_release);
    BL_INFO("[CSharp][AchievementLoader] Four native Terraria achievement events installed");
    return true;
}

}
