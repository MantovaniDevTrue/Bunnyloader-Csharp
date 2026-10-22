#include "experiments/csharp/NativeInvasionMailbox.h"
#include "experiments/csharp/NativeInvasionSpawn.h"
#ifdef __ANDROID__
#include "core/Log.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {

constexpr size_t kMaxPending = 32;
constexpr size_t kMaxReplies = 64;

struct PendingSpawn {
    uint64_t session = 0;
    int epoch = 0;
    int ticket = 0;
    int player = -1;
    float x = 0.f;
    float y = 0.f;
    std::string mod;
    std::string npc;
};

struct SpawnReply {
    uint64_t session = 0;
    int epoch = 0;
    int ticket = 0;
    int error = 0;
    int slot = -1;
    int64_t identity = 0;
    std::string mod;
};

std::mutex g_mutex;
std::deque<PendingSpawn> g_requests;
std::deque<SpawnReply> g_replies;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};

#ifdef __ANDROID__
const char* spawnErrorName(InvasionSpawnError error) {
    switch (error) {
        case InvasionSpawnError::None: return "None";
        case InvasionSpawnError::InvalidRequest: return "InvalidRequest";
        case InvasionSpawnError::NotAuthoritative: return "NotAuthoritative";
        case InvasionSpawnError::WorldUnavailable: return "WorldUnavailable";
        case InvasionSpawnError::UnregisteredNpc: return "UnregisteredNpc";
        case InvasionSpawnError::NativeUnavailable: return "NativeUnavailable";
        case InvasionSpawnError::NativeException: return "NativeException";
        case InvasionSpawnError::NoNpcSlot: return "NoNpcSlot";
        case InvasionSpawnError::VerificationFailed: return "VerificationFailed";
        case InvasionSpawnError::NoSafeSpawnSite: return "NoSafeSpawnSite";
    }
    return "Unknown";
}
#endif

bool validName(const char* value, size_t limit) {
    if (!value) return false;
    const size_t size = strnlen(value, limit + 1);
    if (!size || size > limit) return false;
    for (size_t i = 0; i < size; ++i)
        if (static_cast<unsigned char>(value[i]) < 33 ||
            value[i] == '|' || value[i] == '\\') return false;
    return true;
}

bool sameRequest(const PendingSpawn& a, const PendingSpawn& b) {
    return a.session == b.session && a.epoch == b.epoch &&
           a.ticket == b.ticket && a.mod == b.mod;
}

}

void setNativeInvasionWorldState(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready)) {
        g_requests.clear();
        g_replies.clear();
    }
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session != 0, std::memory_order_release);
}

void clearNativeInvasionMailbox() {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_requests.clear();
    g_replies.clear();
    g_ready.store(false, std::memory_order_release);
}

void pumpNativeInvasionSpawnsOnGameThread() {
    if (!g_ready.load(std::memory_order_acquire)) return;
#ifdef __ANDROID__
    static uint64_t reportedSession = 0;
    static std::array<bool, 10> errorsReported{};
    static bool firstSuccessReported = false;
    const uint64_t liveSession = g_session.load(std::memory_order_acquire);
    if (reportedSession != liveSession) {
        reportedSession = liveSession;
        errorsReported.fill(false);
        firstSuccessReported = false;
    }
#endif
    for (int i = 0; i < 2; ++i) {
        PendingSpawn task;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_requests.empty() || g_replies.size() >= kMaxReplies) return;
            task = std::move(g_requests.front());
            g_requests.pop_front();
        }
        InvasionSpawnResult result;
        if (g_ready.load(std::memory_order_acquire) &&
            g_session.load(std::memory_order_acquire) == task.session) {
            InvasionSpawnRequest request;
            request.modId = task.mod;
            request.npcClass = task.npc;
            request.worldReady = true;
            request.epoch = task.epoch;
            request.ticket = task.ticket;
            request.playerIndex = task.player;
            request.x = task.x;
            request.y = task.y;
            result = spawnNativeInvasionNpc(request);
        } else {
            result.error = InvasionSpawnError::WorldUnavailable;
        }
#ifdef __ANDROID__
        const int code = static_cast<int>(result.error);
        if (result && !firstSuccessReported) {
            firstSuccessReported = true;
            BL_INFO("[CSharp][Invasion] Stage=NativeSpawn Result=Created Mod=%s NPC=%s Slot=%d",
                task.mod.c_str(), task.npc.c_str(), result.npcSlot);
        } else if (!result && code >= 0 &&
                   static_cast<size_t>(code) < errorsReported.size() &&
                   !errorsReported[static_cast<size_t>(code)]) {
            errorsReported[static_cast<size_t>(code)] = true;
            BL_WARN("[CSharp][Invasion] Stage=NativeSpawn Result=Rejected Mod=%s NPC=%s Reason=%s",
                task.mod.c_str(), task.npc.c_str(), spawnErrorName(result.error));
        }
#endif
        std::lock_guard<std::mutex> guard(g_mutex);
        if (!g_ready.load(std::memory_order_acquire) ||
            g_session.load(std::memory_order_acquire) != task.session) continue;
        g_replies.push_back({task.session, task.epoch, task.ticket,
                             static_cast<int>(result.error), result.npcSlot,
                             static_cast<int64_t>(result.identity), task.mod});
    }
}

}

extern "C" __attribute__((visibility("default")))
int64_t bunny_invasion_world_session() {
    if (!bl::csharp::g_ready.load(std::memory_order_acquire)) return 0;
    return static_cast<int64_t>(bl::csharp::g_session.load(std::memory_order_acquire));
}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_submit(int64_t session, int epoch, int ticket,
                          const char* mod, const char* npc,
                          int player, float x, float y) {
    using namespace bl::csharp;
    if (session <= 0 || epoch <= 0 || ticket <= 0 ||
        !validName(mod, 128) || !validName(npc, 128)) return 0;
    InvasionSpawnRequest check;
    check.modId = mod;
    check.npcClass = npc;
    check.worldReady = true;
    check.epoch = epoch;
    check.ticket = ticket;
    check.playerIndex = player;
    check.x = x;
    check.y = y;
    if (!canDispatchInvasionSpawn(check, 0)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session) ||
        g_requests.size() >= kMaxPending || g_replies.size() >= kMaxReplies)
        return 0;
    PendingSpawn task{static_cast<uint64_t>(session), epoch, ticket, player,
                      x, y, mod, npc};
    for (const auto& candidate : g_requests)
        if (sameRequest(candidate, task)) return 0;
    for (const auto& reply : g_replies)
        if (reply.session == task.session && reply.epoch == task.epoch &&
            reply.ticket == task.ticket && reply.mod == task.mod) return 0;
    g_requests.push_back(std::move(task));
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_poll(int64_t session, const char* mod,
                        int* epoch, int* ticket, int* error,
                        int* slot, int64_t* identity) {
    using namespace bl::csharp;
    if (session <= 0 || !validName(mod, 128) || !epoch || !ticket ||
        !error || !slot || !identity) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    const auto it = std::find_if(g_replies.begin(), g_replies.end(),
        [&](const SpawnReply& reply) {
            return reply.session == static_cast<uint64_t>(session) && reply.mod == mod;
        });
    if (it == g_replies.end()) return 0;
    *epoch = it->epoch;
    *ticket = it->ticket;
    *error = it->error;
    *slot = it->slot;
    *identity = it->identity;
    g_replies.erase(it);
    return 1;
}
