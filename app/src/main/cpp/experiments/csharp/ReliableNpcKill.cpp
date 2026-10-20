#include "experiments/csharp/ReliableNpcKill.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <atomic>
#include <deque>
#include <mutex>

namespace bl::csharp {
namespace {
struct Kill {
    int type;
    int slot;
    int generation;
    uintptr_t identity;
};

std::mutex g_mutex;
std::deque<Kill> g_pending;
std::atomic<uint64_t> g_dropped{0};

bool submit(const Kill& value) {
    return queueManagedEntityCombatEvent(3, value.type, value.slot,
        value.generation, -1, value.identity, 0, -1);
}
}

bool queueReliableNpcKill(int type, int slot, int generation, uintptr_t identity) {
    if (type < 0 || type >= 32768 || slot < 0 || slot >= 512 ||
        generation <= 0 || identity == 0) return false;
    const Kill value{type, slot, generation, identity};
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (const auto& queued : g_pending)
            if (queued.slot == slot && queued.type == type &&
                queued.generation == generation && queued.identity == identity)
                return true;
        if (!g_pending.empty()) {
            if (g_pending.size() >= 512) {
                g_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            g_pending.push_back(value);
            return true;
        }
    }
    if (submit(value)) return true;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_pending.size() >= 512) {
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    g_pending.push_back(value);
    return true;
}

void pumpReliableNpcKillsOnGameThread() {
    for (int i = 0; i < 16; ++i) {
        std::lock_guard<std::mutex> guard(g_mutex);
        if (g_pending.empty() || !submit(g_pending.front())) return;
        g_pending.pop_front();
    }
}

void resetReliableNpcKills() {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_pending.clear();
}

size_t pendingReliableNpcKills() {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_pending.size();
}

uint64_t droppedReliableNpcKills() {
    return g_dropped.load(std::memory_order_relaxed);
}
}
