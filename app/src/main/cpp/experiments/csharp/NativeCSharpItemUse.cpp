#include "experiments/csharp/NativeCSharpItemUse.h"
#include "experiments/csharp/WorldAuthority.h"
#include "content/common/GameRefs.h"
#include "content/items/ModItems.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "menu/NetRequests.h"
#include "content/common/GameRefs.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace bl::csharp {
namespace {

constexpr size_t kBindingsLimit = 64;
constexpr size_t kEventsLimit = 64;

struct ItemBinding {
    uint64_t session = 0;
    std::string mod;
    std::string name;
};
struct ItemUseEvent {
    uint64_t session = 0;
    std::string mod;
    std::string name;
    int player = -1;
};

std::mutex g_mutex;
std::deque<ItemBinding> g_requests;
std::deque<ItemUseEvent> g_events;
std::map<int, ItemBinding> g_installed;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};
std::atomic<unsigned> g_dropped{0};

using ActualUseFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
ActualUseFn g_originalUse = nullptr;
bool g_hookInstalled = false;
int g_itemTypeOffset = -1;
int g_playerIndexOffset = -1;

bool validName(const char* name, size_t maxLength) {
    if (!name) return false;
    const size_t size = strnlen(name, maxLength + 1);
    if (size == 0 || size > maxLength) return false;
    for (size_t i = 0; i < size; ++i) {
        const char ch = name[i];
        if (!(ch >= 'a' && ch <= 'z') && !(ch >= 'A' && ch <= 'Z') &&
            !(ch >= '0' && ch <= '9') && ch != '_' && ch != '-')
            return false;
    }
    return true;
}

bool sameBinding(const ItemBinding& a, const ItemBinding& b) {
    return a.session == b.session && a.mod == b.mod && a.name == b.name;
}

void onActualUse(Il2CppObject* player, Il2CppObject* item, const MethodInfo* method) {
    int itemType = -1;
    int playerIndex = -1;
    if (player && item && g_ready.load(std::memory_order_acquire) &&
        g_playerIndexOffset >= 0 && g_itemTypeOffset >= 0) {
        itemType = runtime::field<int32_t>(item, g_itemTypeOffset);
        playerIndex = runtime::field<int32_t>(player, g_playerIndexOffset);
    }
    if (g_originalUse) g_originalUse(player, item, method);
    if (itemType < 0 || playerIndex < 0 || playerIndex >= 255 ||
        !g_ready.load(std::memory_order_acquire)) return;
    const auto binding = g_installed.find(itemType);
    if (binding == g_installed.end() ||
        binding->second.session != g_session.load(std::memory_order_acquire)) return;
    const int mode = runtime::netMode();
    if (mode == 1) {
        runtime::sendToServer("invuse " + binding->second.mod + " " + binding->second.name);
        return;
    }
    if (!worldIsAuthoritative(mode)) return;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_events.size() >= kEventsLimit) {
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    g_events.push_back({binding->second.session, binding->second.mod,
                        binding->second.name, playerIndex});
}

bool installItemUseHook() {
    if (g_hookInstalled) return true;
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    auto* item = il2cpp::findClass({"Terraria", "Item", {}});
    if (!player || !item) return false;
    const int itemTypeOffset = il2cpp::fieldOffset(item, "type");
    const int playerIndexOffset = il2cpp::fieldOffset(player, "whoAmI");
    if (itemTypeOffset < 0 || playerIndexOffset < 0) return false;
    const auto* method = il2cpp::findMethodBySignature(player,
        il2cpp::parseSignature("void ItemCheck_StartActualUse(Item sItem)"));
    if (!method || !hook::install(method, onActualUse, &g_originalUse))
        return false;
    g_itemTypeOffset = itemTypeOffset;
    g_playerIndexOffset = playerIndexOffset;
    g_hookInstalled = true;
    return true;
}

}

bool enqueueVerifiedRemoteItemUse(int playerIndex, const std::string& mod,
                                  const std::string& itemName) {
    if (!runtime::isNetHost() || !g_ready.load(std::memory_order_acquire) ||
        !validName(mod.c_str(), 128) || !validName(itemName.c_str(), 128) ||
        playerIndex < 0 || playerIndex >= 255) return false;
    const int type = runtime::modItemTypeByKey(mod + "/" + itemName);
    const auto bound = g_installed.find(type);
    if (type < runtime::kVanillaItemCount || bound == g_installed.end() ||
        bound->second.mod != mod || bound->second.name != itemName ||
        bound->second.session != g_session.load(std::memory_order_acquire))
        return false;

    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* klass = il2cpp::findClass({"Terraria", "Player", {}});
    if (!main || !klass) return false;
    auto* playersField = il2cpp::findField(main, "player");
    const int inventory = il2cpp::fieldOffset(klass, "inventory");
    const int selected = il2cpp::fieldOffset(klass, "selectedItem");
    const int active = il2cpp::fieldOffset(klass, "active");
    if (!playersField || inventory < 0 || selected < 0 || active < 0) return false;
    Il2CppArray* players = nullptr;
    il2cpp::api().field_static_get_value(playersField, &players);
    if (!players || static_cast<uintptr_t>(playerIndex) >= players->length) return false;
    Il2CppObject* player = static_cast<Il2CppObject**>(arrayData(players))[playerIndex];
    if (!player || runtime::field<uint8_t>(player, active) == 0) return false;
    Il2CppArray* items = runtime::field<Il2CppArray*>(player, inventory);
    const int selectedSlot = runtime::field<int32_t>(player, selected);
    if (!items || selectedSlot < 0 ||
        static_cast<uintptr_t>(selectedSlot) >= items->length) return false;
    Il2CppObject* selectedItem =
        static_cast<Il2CppObject**>(arrayData(items))[selectedSlot];
    if (!selectedItem || g_itemTypeOffset < 0 ||
        runtime::field<int32_t>(selectedItem, g_itemTypeOffset) != type) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_events.size() >= kEventsLimit) return false;
    g_events.push_back({g_session.load(std::memory_order_acquire), mod,
                        itemName, playerIndex});
    return true;
}

void setNativeItemUseWorldState(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready)) {
        g_requests.clear();
        g_events.clear();
        g_installed.clear();
    }
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session > 0, std::memory_order_release);
}

void pumpNativeItemUseBindingsOnGameThread() {
    if (!g_ready.load(std::memory_order_acquire)) return;
    for (int i = 0; i < 4; ++i) {
        ItemBinding binding;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_requests.empty()) return;
            binding = std::move(g_requests.front());
            g_requests.pop_front();
        }
        if (binding.session != g_session.load(std::memory_order_acquire)) continue;
        if (!runtime::modItemsSettled()) {
            std::lock_guard<std::mutex> guard(g_mutex);
            g_requests.push_front(std::move(binding));
            return;
        }
        const int type = runtime::modItemTypeByKey(binding.mod + "/" + binding.name);
        if (type < runtime::kVanillaItemCount || !installItemUseHook()) continue;
        std::lock_guard<std::mutex> guard(g_mutex);
        if (g_ready.load(std::memory_order_acquire) &&
            binding.session == g_session.load(std::memory_order_acquire))
            g_installed[type] = std::move(binding);
    }
}

}

extern "C" __attribute__((visibility("default")))
int bunny_item_use_bind(int64_t session, const char* mod, const char* itemName) {
    using namespace bl::csharp;
    if (session <= 0 || !validName(mod, 128) || !validName(itemName, 128))
        return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session) ||
        g_requests.size() >= kBindingsLimit) return 0;
    ItemBinding request{static_cast<uint64_t>(session), mod, itemName};
    for (const auto& entry : g_installed)
        if (sameBinding(entry.second, request)) return 1;
    for (const auto& other : g_requests)
        if (sameBinding(other, request)) return 1;
    g_requests.push_back(std::move(request));
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_item_use_ready(int64_t session, const char* mod, const char* itemName) {
    using namespace bl::csharp;
    if (session <= 0 || !validName(mod, 128) || !validName(itemName, 128))
        return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    const ItemBinding target{static_cast<uint64_t>(session), mod, itemName};
    for (const auto& entry : g_installed)
        if (sameBinding(entry.second, target)) return 1;
    return 0;
}

extern "C" __attribute__((visibility("default")))
int bunny_item_use_poll(int64_t session, const char* mod, const char* itemName,
                        int* playerIndex) {
    using namespace bl::csharp;
    if (session <= 0 || !validName(mod, 128) ||
        !validName(itemName, 128) || !playerIndex) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    const auto it = std::find_if(g_events.begin(), g_events.end(),
        [&](const ItemUseEvent& value) {
            return value.session == static_cast<uint64_t>(session) &&
                value.mod == mod && value.name == itemName;
        });
    if (it == g_events.end()) return 0;
    *playerIndex = it->player;
    g_events.erase(it);
    return 1;
}
