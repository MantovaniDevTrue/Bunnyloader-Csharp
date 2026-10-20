#include "experiments/csharp/NativeInvasionRewards.h"
#include "content/items/ModItems.h"
#include "menu/Cheats.h"
#include "menu/NetRequests.h"

#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

namespace bl::csharp {
namespace {

struct Reward {
    uint64_t session = 0;
    int epoch = 0;
    std::string mod;
    std::string invasion;
    std::string item;
    int amount = 0;
    int player = -1;
};

using Identity = std::tuple<uint64_t, std::string, std::string, int>;

std::mutex g_mutex;
std::deque<Reward> g_requests;
std::set<Identity> g_processed;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};

bool valid(const char* value, size_t max) {
    if (!value) return false;
    const size_t length = strnlen(value, max + 1);
    if (length == 0 || length > max) return false;
    for (size_t i = 0; i < length; ++i) {
        const char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

Identity key(const Reward& reward) {
    return {reward.session, reward.mod, reward.invasion, reward.epoch};
}
}

void setNativeInvasionRewardWorldState(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready)) {
        g_requests.clear();
        g_processed.clear();
    }
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session != 0, std::memory_order_release);
}

void pumpNativeInvasionRewardsOnGameThread() {
    if (!g_ready.load(std::memory_order_acquire) ||
        !(runtime::netMode() == 0 || runtime::isNetHost()) ||
        !runtime::modItemsSettled()) return;
    for (int i = 0; i < 2; ++i) {
        Reward reward;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_requests.empty()) return;
            reward = g_requests.front();
        }
        const int type = runtime::modItemTypeByKey(reward.mod + "/" + reward.item);
        bool validType = type >= runtime::kVanillaItemCount &&
            type < runtime::itemTypeCount() &&
            runtime::modItemKey(type) == reward.mod + "/" + reward.item;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_requests.empty() ||
                g_session.load(std::memory_order_acquire) != reward.session)
                return;
            if (key(g_requests.front()) != key(reward)) continue;
            g_requests.pop_front();
            if (!validType) continue;
            g_processed.insert(key(reward));
        }
        runtime::giveItem(type, reward.amount, reward.player);
    }
}

}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_reward_submit(int64_t session, int epoch, const char* mod,
                                 const char* invasion, const char* item,
                                 int amount, int player) {
    using namespace bl::csharp;
    if (session <= 0 || epoch <= 0 || amount < 1 || amount > 999 ||
        player < 0 || player >= 255 || !valid(mod, 128) ||
        !valid(invasion, 64) || !valid(item, 128)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    Reward reward{static_cast<uint64_t>(session), epoch, mod, invasion,
                  item, amount, player};
    const Identity id = key(reward);
    if (g_processed.contains(id)) return 1;
    for (const Reward& entry : g_requests)
        if (key(entry) == id) return 1;
    if (g_requests.size() >= 32) return 0;
    g_requests.push_back(std::move(reward));
    return 1;
}
