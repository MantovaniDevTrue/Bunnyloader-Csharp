#include "experiments/csharp/NpcAiHost.h"

#include "content/common/GameRefs.h"
#include "content/npcs/ModNpcs.h"
#include "core/Log.h"
#include "experiments/csharp/CSharpModLoader.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ReliableNpcKill.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/TerrariaHost.h"
#include "experiments/csharp/WorldAuthority.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <string>

namespace bl::csharp {
namespace {

struct Vector2 { float x, y; };

struct Refs {
    bool attempted = false;
    bool ok = false;
    FieldInfo* netMode = nullptr;
    FieldInfo* gameMenu = nullptr;
    int type = -1;
    int active = -1;
    int life = -1;
    int lifeMax = -1;
    int direction = -1;
    int spriteDirection = -1;
    const MethodInfo* ai = nullptr;
    const MethodInfo* checkActive = nullptr;
    const MethodInfo* loot = nullptr;
};

Refs& refs() {
    static Refs r;
    if (r.ok) return r;
    r = {};
    r.attempted = true;
    using namespace il2cpp;
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    if (!npc || !main || !runtime::resolveGameRefs()) return r;

    r.netMode = findField(main, "netMode");
    r.gameMenu = findField(main, "gameMenu");
    r.type = fieldOffset(npc, "type");
    r.active = fieldOffset(npc, "active");
    r.life = fieldOffset(npc, "life");
    r.lifeMax = fieldOffset(npc, "lifeMax");
    r.direction = fieldOffset(npc, "direction");
    r.spriteDirection = fieldOffset(npc, "spriteDirection");
    r.ai = api().class_get_method_from_name(npc, "AI", 0);
    r.checkActive = api().class_get_method_from_name(npc, "CheckActive", 0);
    r.loot = api().class_get_method_from_name(npc, "NPCLoot", 0);
    r.ok = r.netMode && r.type >= 0 && r.active >= 0 &&
           r.life >= 0 && r.lifeMax >= 0 && r.ai;
    if (!r.ok) BL_WARN("csharp NPC: refs incompletas; hook nao instalado");
    return r;
}

using NpcFn = void (*)(Il2CppObject*, const MethodInfo*);
NpcFn g_origAi = nullptr;
NpcFn g_origCheckActive = nullptr;
NpcFn g_origLoot = nullptr;
bool g_installed = false;
bool g_liveNpcInstallRequested = false;
std::atomic<bool> g_buffDispatchRequested{false};
std::atomic<bool> g_liveNpcEnabled{false};
std::mutex g_liveNpcMutex;
LiveNpcAiStatus g_liveNpcStatus;
std::string g_liveNpcPath;

constexpr int kLiveNpcMaxCalls = 24;
constexpr int kLiveNpcSampleInterval = 12;
constexpr int kLiveNpcWorkerTimeoutMs = 300;

struct LiveNpcJob {
    std::string modId;
    std::string assemblyPath;
    uintptr_t npcPointer = 0;
    uint64_t session = 0;
    int slot = -1;
    int type = -1;
    int deltaX = 0;
    int life = 0;
};

struct LiveNpcCompletion {
    LiveNpcJob job;
    bool ok = false;
    int velocityTenths = 0;
    double callbackMs = 0.0;
};

std::mutex g_liveDispatchMutex;
std::condition_variable g_liveDispatchCv;
LiveNpcJob g_liveJob;
LiveNpcCompletion g_liveCompletion;
bool g_liveHasJob = false;
bool g_liveWorkerBusy = false;
bool g_liveHasCompletion = false;
bool g_liveWorkerStarted = false;
uint64_t g_liveSession = 0;
std::atomic<int> g_liveSampleCounter{0};
std::chrono::steady_clock::time_point g_liveJobTime{};

void liveNpcWorkerLoop() {
    for (;;) {
        LiveNpcJob job;
        {
            std::unique_lock<std::mutex> lock(g_liveDispatchMutex);
            g_liveDispatchCv.wait(lock, [] { return g_liveHasJob; });
            job = std::move(g_liveJob);
            g_liveHasJob = false;
        }
        int velocity = 0;
        const auto start = std::chrono::steady_clock::now();
        BL_INFO("csharp live NPC worker: begin session=%llu slot=%d life=%d",
                static_cast<unsigned long long>(job.session), job.slot, job.life);
        const bool ok = invokeLiveSourceNpcAi(
            job.modId, job.assemblyPath, job.deltaX, job.life, &velocity);
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        {
            std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
            if (job.session == g_liveSession) {
                g_liveCompletion = {job, ok, velocity, ms};
                g_liveHasCompletion = true;
            }
            g_liveWorkerBusy = false;
        }
        BL_INFO("csharp live NPC worker: session=%llu slot=%d ok=%d elapsedMs=%.3f",
                static_cast<unsigned long long>(job.session), job.slot, ok, ms);
    }
}



struct Instance {
    Il2CppObject* object = nullptr;
    int type = -1;
    int generation = 0;
};

std::array<Instance, 512> g_instances{};
int g_nextGeneration = 1;
int g_spawns = 0;
int g_releases = 0;
int g_calls = 0;
int g_writes = 0;
int g_attacksQueued = 0;
int g_attacksConsumed = 0;
int g_attacksRejected = 0;

struct PendingAttack {
    int type = -1;
    int generation = 0;
    bool ready = false;
    int ttl = 0;
    HostNpcAttack attack{};
};

std::array<PendingAttack, 512> g_pendingAttacks{};
std::array<int, 512> g_attackDelay{};

bool validAttack(const HostNpcAttack& attack, const HostNpcState& state) {
    if (attack.count < 1 || attack.count > 2 ||
        attack.damage < 1 || attack.damage > 100 ||
        !std::isfinite(attack.x) || !std::isfinite(attack.y) ||
        std::fabs(attack.x - state.x) > 32.f || std::fabs(attack.y - state.y) > 32.f)
        return false;

    const auto checkVelocity = [](float vx, float vy) {
        if (!std::isfinite(vx) || !std::isfinite(vy)) return false;
        const float speedSq = vx * vx + vy * vy;
        return speedSq >= 4.f && speedSq <= 256.f;
    };
    return checkVelocity(attack.vx0, attack.vy0) &&
           (attack.count == 1 || checkVelocity(attack.vx1, attack.vy1));
}

void retire(int slot, Il2CppObject* expected = nullptr) {
    if (slot < 0 || static_cast<size_t>(slot) >= g_instances.size()) return;
    Instance& entry = g_instances[static_cast<size_t>(slot)];
    if (!entry.object || (expected && entry.object != expected)) return;
    const Instance previous = entry;
    entry = {};
    g_pendingAttacks[static_cast<size_t>(slot)] = {};
    g_attackDelay[static_cast<size_t>(slot)] = 0;
    csharpmods::releaseOwnedNpc(previous.type, slot, previous.generation);
    ++g_releases;
}

void retireObject(Il2CppObject* npc) {
    if (!npc) return;
    const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
    retire(slot, npc);
}

void runNativeAi(Il2CppObject* npc) {
    const Refs& r = refs();
    if (!r.ok || !npc) return;

    int netMode = -1;
    il2cpp::api().field_static_get_value(r.netMode, &netMode);
    if (!worldIsAuthoritative(netMode)) return;

    const int type = runtime::field<int>(npc, r.type);
    if (!runtime::isModNpc(type) || !csharpmods::hasOwnedNpcBehavior(type)) return;
    if (!runtime::field<uint8_t>(npc, r.active) ||
        runtime::field<int>(npc, r.life) <= 0) {
        retireObject(npc);
        return;
    }

    const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_instances.size()) return;
    TerrariaSnapshot snapshot = captureTerrariaSnapshot(false, false);
    if (!snapshot.ready || snapshot.player.life <= 0) return;

    Instance& entry = g_instances[static_cast<size_t>(slot)];
    PendingAttack& pending = g_pendingAttacks[static_cast<size_t>(slot)];
    if (pending.ready && --pending.ttl <= 0) pending = {};
    if (entry.object != npc || entry.type != type) {
        retire(slot);
        if (g_nextGeneration <= 0) g_nextGeneration = 1;
        entry = {npc, type, g_nextGeneration++};
        ++g_spawns;
    }

    const auto& g = runtime::game();
    const Vector2 pos = runtime::field<Vector2>(npc, g.entity.position);
    const Vector2 vel = runtime::field<Vector2>(npc, g.entity.velocity);
    const int width = runtime::field<int>(npc, g.entity.width);
    const int height = runtime::field<int>(npc, g.entity.height);

    HostNpcState state{};
    state.slot = slot;
    state.type = type;
    state.generation = entry.generation;
    state.life = runtime::field<int>(npc, r.life);
    state.lifeMax = runtime::field<int>(npc, r.lifeMax);
    state.targetPlayer = snapshot.player.index;
    state.x = pos.x + width * 0.5f;
    state.y = pos.y + height * 0.5f;
    state.vx = vel.x;
    state.vy = vel.y;

    ++g_calls;
    if (!csharpmods::dispatchOwnedNpc(type, &state, snapshot.player.x, snapshot.player.y))
        return;

    if (!std::isfinite(state.vx) || !std::isfinite(state.vy) ||
        std::fabs(state.vx) > 32.f || std::fabs(state.vy) > 32.f ||
        !runtime::field<uint8_t>(npc, r.active) ||
        runtime::field<int>(npc, r.life) <= 0 ||
        runtime::field<int>(npc, r.type) != type ||
        runtime::field<int>(npc, g.entity.whoAmI) != slot ||
        entry.object != npc || entry.generation != state.generation) return;

    runtime::field<Vector2>(npc, g.entity.velocity) = {state.vx, state.vy};
    if (r.direction >= 0 && std::fabs(state.vx) > 0.1f) {
        const int facing = state.vx >= 0.f ? 1 : -1;
        runtime::field<int>(npc, r.direction) = facing;
        if (r.spriteDirection >= 0)
            runtime::field<int>(npc, r.spriteDirection) = facing;
    }
    ++g_writes;

    if (g_attackDelay[static_cast<size_t>(slot)] > 0) {
        --g_attackDelay[static_cast<size_t>(slot)];
        return;
    }

    HostNpcAttack attack{};
    if (!csharpmods::dispatchOwnedNpcCombat(type, &state, snapshot.player.x,
                                             snapshot.player.y, &attack)) return;
    if (!validAttack(attack, state)) {
        ++g_attacksRejected;
        return;
    }
    g_pendingAttacks[static_cast<size_t>(slot)] = {type, entry.generation, true, 4, attack};
    g_attackDelay[static_cast<size_t>(slot)] = 12;
    ++g_attacksQueued;
}

struct ManagedNpcSlot {
    Il2CppObject* object = nullptr;
    int type = -1;
    int generation = 0;
    int lastLife = 0;
    bool killQueued = false;
};
std::array<ManagedNpcSlot, 512> g_managedSourceNpcSlots{};
int g_managedSourceNpcGeneration = 1;

void releaseManagedSourceNpc(Il2CppObject* npc) {
    if (!npc || !runtime::resolveGameRefs()) return;
    const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_managedSourceNpcSlots.size()) return;
    if (g_managedSourceNpcSlots[static_cast<size_t>(slot)].object == npc)
        g_managedSourceNpcSlots[static_cast<size_t>(slot)] = {};
}

void runManagedSourceNpcAi(Il2CppObject* npc) {
    if (!npc || !refs().ok || !runtime::resolveGameRefs()) return;
    const Refs& r = refs();
    const int type = runtime::field<int>(npc, r.type);
    if (!hasManagedEntityNpcType(type) || csharpmods::hasOwnedNpcBehavior(type)) return;
    if (!runtime::field<uint8_t>(npc, r.active) ||
        runtime::field<int>(npc, r.life) <= 0) {
        releaseManagedSourceNpc(npc);
        return;
    }
    int netMode = -1;
    il2cpp::api().field_static_get_value(r.netMode, &netMode);
    if (!worldIsAuthoritative(netMode)) return;
    const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_managedSourceNpcSlots.size()) return;
    ManagedNpcSlot& identity = g_managedSourceNpcSlots[static_cast<size_t>(slot)];
    const int life = runtime::field<int>(npc, r.life);
    if (identity.object != npc || identity.type != type) {
        if (g_managedSourceNpcGeneration <= 0) g_managedSourceNpcGeneration = 1;
        identity = {npc, type, g_managedSourceNpcGeneration++, life, false};
    } else if (identity.lastLife > life && !identity.killQueued) {
        queueManagedEntityCombatEvent(2, type, slot, identity.generation, -1,
                                      reinterpret_cast<uintptr_t>(npc),
                                      std::min(identity.lastLife - life, 1000000), -1);
        identity.lastLife = life;
    } else {
        identity.lastLife = life;
    }
    const TerrariaSnapshot player = captureTerrariaSnapshot(false, false);
    if (!player.ready || player.player.life <= 0) return;
    const auto& game = runtime::game();
    const Vector2 pos = runtime::field<Vector2>(npc, game.entity.position);
    const Vector2 before = runtime::field<Vector2>(npc, game.entity.velocity);
    const int width = runtime::field<int>(npc, game.entity.width);
    const int height = runtime::field<int>(npc, game.entity.height);
    ManagedEntityVelocity next{};
    if (!tickManagedEntityNpc(type, slot, identity.generation,
                              reinterpret_cast<uintptr_t>(npc),
                              runtime::field<int>(npc, r.life),
                              runtime::field<int>(npc, r.lifeMax),
                              pos.x + width * 0.5f, pos.y + height * 0.5f,
                              before.x, before.y, player.player.x,
                              player.player.y, &next)) return;
    if (runtime::field<int>(npc, r.type) != type ||
        runtime::field<int>(npc, game.entity.whoAmI) != slot ||
        !runtime::field<uint8_t>(npc, r.active) ||
        runtime::field<int>(npc, r.life) <= 0 ||
        identity.object != npc) return;
    runtime::field<Vector2>(npc, game.entity.velocity) = {next.x, next.y};
    if (r.direction >= 0 && std::fabs(next.x) > 0.1f) {
        const int facing = next.x >= 0.f ? 1 : -1;
        runtime::field<int>(npc, r.direction) = facing;
        if (r.spriteDirection >= 0)
            runtime::field<int>(npc, r.spriteDirection) = facing;
    }
}

void runLiveNpcAi(Il2CppObject* npc) {
    if (!g_liveNpcEnabled.load(std::memory_order_acquire) || !npc) return;
    const auto& r = refs();
    if (!r.ok) return;
    const int type = runtime::field<int>(npc, r.type);
    {
        std::lock_guard<std::mutex> lock(g_liveNpcMutex);
        if (type != g_liveNpcStatus.npcType) return;
    }
    bool inMenu = false;
    if (r.gameMenu) il2cpp::api().field_static_get_value(r.gameMenu, &inMenu);
    if (inMenu) {
        pauseLiveNpcAi();
        std::lock_guard<std::mutex> lock(g_liveNpcMutex);
        g_liveNpcStatus.phase = "paused-world-transition";
        return;
    }
    int netMode = -1;
    il2cpp::api().field_static_get_value(r.netMode, &netMode);
    if (!worldIsAuthoritative(netMode) || !runtime::field<uint8_t>(npc, r.active)) return;
    const auto& game = runtime::game();
    const int slot = runtime::field<int>(npc, game.entity.whoAmI);
    if (slot < 0 || slot >= 512) return;
    const int life = runtime::field<int>(npc, r.life);
    if (life <= 0) return;

    LiveNpcCompletion completed;
    bool haveCompletion = false;
    bool timedOut = false;
    {
        std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
        if (g_liveHasCompletion) {
            completed = std::move(g_liveCompletion);
            g_liveHasCompletion = false;
            haveCompletion = true;
        }
        if (g_liveWorkerBusy && !g_liveHasJob &&
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - g_liveJobTime).count() >
                kLiveNpcWorkerTimeoutMs) timedOut = true;
    }
    if (timedOut) {
        g_liveNpcEnabled.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lock(g_liveNpcMutex);
        g_liveNpcStatus.phase = "paused-worker-timeout";
        ++g_liveNpcStatus.errors;
        return;
    }
    if (haveCompletion) {
        std::lock_guard<std::mutex> lock(g_liveNpcMutex);
        auto& status = g_liveNpcStatus;
        ++status.events;
        status.lastSlot = completed.job.slot;
        status.lastDeltaX = completed.job.deltaX;
        status.maxCallbackMs = std::max(status.maxCallbackMs, completed.callbackMs);
        if (!completed.ok) {
            ++status.errors;
            status.phase = "callback-error";
        } else if (completed.job.slot == slot &&
                   completed.job.npcPointer == reinterpret_cast<uintptr_t>(npc) &&
                   completed.job.type == type &&
                   completed.job.life == life &&
                   runtime::field<int>(npc, r.life) > 0 &&
                   runtime::field<uint8_t>(npc, r.active)) {
            const Vector2 previous =
                runtime::field<Vector2>(npc, game.entity.velocity);
            if (std::isfinite(previous.x) && std::isfinite(previous.y)) {
                const float horizontal = std::clamp(
                    previous.x * 0.7f + completed.velocityTenths * 0.03f,
                    -6.f, 6.f);
                runtime::field<Vector2>(npc, game.entity.velocity) = {
                    horizontal, previous.y};
                status.lastVelocityTenths = completed.velocityTenths;
                ++status.writes;
                if (std::fabs(horizontal) > 0.1f && r.direction >= 0) {
                    const int direction = horizontal >= 0.f ? 1 : -1;
                    runtime::field<int>(npc, r.direction) = direction;
                    if (r.spriteDirection >= 0)
                        runtime::field<int>(npc, r.spriteDirection) = direction;
                }
                status.phase = "running-async";
            }
        } else {
            ++status.discarded;
        }
        if (status.errors >= 3 ||
            status.events >= kLiveNpcMaxCalls ||
            (status.events > 2 && completed.callbackMs > 20.0)) {
            g_liveNpcEnabled.store(false, std::memory_order_release);
            status.limitReached = status.events >= kLiveNpcMaxCalls;
            status.phase = status.limitReached ? "completed-limit" :
                status.errors >= 3 ? "disabled-errors" : "paused-slow-callback";
            return;
        }
    }

    if (!g_liveNpcEnabled.load(std::memory_order_acquire) ||
        (g_liveSampleCounter.fetch_add(1, std::memory_order_relaxed) + 1) % kLiveNpcSampleInterval != 0) return;
    const TerrariaSnapshot snapshot = captureTerrariaSnapshot(false, false);
    if (!snapshot.ready || snapshot.player.life <= 0) return;
    const Vector2 pos = runtime::field<Vector2>(npc, game.entity.position);
    const int width = runtime::field<int>(npc, game.entity.width);
    const float dx = snapshot.player.x - (pos.x + width * 0.5f);
    if (!std::isfinite(dx)) return;

    std::string modId;
    std::string path;
    {
        std::lock_guard<std::mutex> lock(g_liveNpcMutex);
        if (!g_liveNpcEnabled.load(std::memory_order_acquire)) return;
        modId = g_liveNpcStatus.modId;
        path = g_liveNpcPath;
    }
    bool submitted = false;
    {
        std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
        if (!g_liveWorkerBusy && !g_liveHasCompletion) {
            g_liveJob = {
                modId, path, reinterpret_cast<uintptr_t>(npc),
                g_liveSession, slot, type,
                static_cast<int>(std::clamp(dx, -100000.f, 100000.f)), life
            };
            g_liveJobTime = std::chrono::steady_clock::now();
            g_liveWorkerBusy = true;
            g_liveHasJob = true;
            submitted = true;
        }
    }
    if (submitted) {
        {
            std::lock_guard<std::mutex> lock(g_liveNpcMutex);
            ++g_liveNpcStatus.queued;
            g_liveNpcStatus.phase = "queued-async";
        }
        g_liveDispatchCv.notify_one();
    }
}

void hookAi(Il2CppObject* npc, const MethodInfo* method) {
    g_origAi(npc, method);
    if (hasPendingManagedNpcBuffEffect())
        processManagedNpcBuffEffect(npc);
    runNativeAi(npc);
    runManagedSourceNpcAi(npc);
    runLiveNpcAi(npc);
}

void hookCheckActive(Il2CppObject* npc, const MethodInfo* method) {
    g_origCheckActive(npc, method);
    if (npc && !runtime::field<uint8_t>(npc, refs().active)) {
        const int type = runtime::field<int>(npc, refs().type);
        const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
        int mode = -1;
        il2cpp::api().field_static_get_value(refs().netMode, &mode);
        if (worldIsAuthoritative(mode) && hasManagedEntityNpcType(type) && slot >= 0 &&
            static_cast<size_t>(slot) < g_managedSourceNpcSlots.size() &&
            runtime::field<int>(npc, refs().life) <= 0) {
            ManagedNpcSlot& state = g_managedSourceNpcSlots[static_cast<size_t>(slot)];
            if (state.object == npc && state.type == type && !state.killQueued) {
                const int life = runtime::field<int>(npc, refs().life);
                if (state.lastLife > life) {
                    queueManagedEntityCombatEvent(2, type, slot, state.generation, -1,
                        reinterpret_cast<uintptr_t>(npc),
                        std::min(state.lastLife - life, 1000000), -1);
                    state.lastLife = life;
                }
                state.killQueued = true;
                queueReliableNpcKill(type, slot, state.generation,
                    reinterpret_cast<uintptr_t>(npc));
            }
        }
        retireObject(npc);
        releaseManagedSourceNpc(npc);
    }
}

void hookLoot(Il2CppObject* npc, const MethodInfo* method) {
    if (npc && refs().ok && runtime::resolveGameRefs()) {
        const int type = runtime::field<int>(npc, refs().type);
        const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
        int mode = -1;
        il2cpp::api().field_static_get_value(refs().netMode, &mode);
        if (worldIsAuthoritative(mode) && hasManagedEntityNpcType(type) && slot >= 0 &&
            static_cast<size_t>(slot) < g_managedSourceNpcSlots.size()) {
            ManagedNpcSlot& state = g_managedSourceNpcSlots[static_cast<size_t>(slot)];
            if (state.object == npc && state.type == type && !state.killQueued) {
                const int life = runtime::field<int>(npc, refs().life);
                if (state.lastLife > life) {
                    queueManagedEntityCombatEvent(2, type, slot, state.generation, -1,
                        reinterpret_cast<uintptr_t>(npc),
                        std::min(state.lastLife - life, 1000000), -1);
                    state.lastLife = life;
                }
                state.killQueued = true;
                queueReliableNpcKill(type, slot, state.generation,
                    reinterpret_cast<uintptr_t>(npc));
            }
        }
    }
    retireObject(npc);
    releaseManagedSourceNpc(npc);
    g_origLoot(npc, method);
}

}

void resetManagedSourceNpcSlots() {
    g_managedSourceNpcSlots.fill({});
}

LiveNpcAiStatus liveNpcAiStatus() {
    std::lock_guard<std::mutex> guard(g_liveNpcMutex);
    LiveNpcAiStatus result = g_liveNpcStatus;
    result.installed = g_installed;
    result.enabled = g_liveNpcEnabled.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
        result.workerPending = g_liveWorkerBusy || g_liveHasCompletion;
        result.workerStarted = g_liveWorkerStarted;
    }
    return result;
}

void pauseLiveNpcAi() {
    g_liveNpcEnabled.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> guard(g_liveNpcMutex);
    if (g_liveNpcStatus.phase == "running-async" ||
        g_liveNpcStatus.phase == "queued-async" ||
        g_liveNpcStatus.phase == "armed")
        g_liveNpcStatus.phase = "paused";
}

bool enableLiveNpcAi(const std::string& modId, int npcType,
                     const std::string& assemblyPath, std::string* error) {
    if (!error) return false;
    if (modId.empty() || assemblyPath.empty() ||
        npcType < runtime::kVanillaNpcCount ||
        !runtime::modNpcBelongsTo(npcType, modId)) {
        *error = "NPC type does not belong to the C# source mod";
        return false;
    }
    const auto live = liveSourceStatus(modId);
    if (!live.loaded || !live.ready || live.failures || live.npcFailures) {
        *error = "Run successful sourceLiveTick for this mod before enabling NPC AI";
        return false;
    }
    {
        std::lock_guard<std::mutex> guard(g_liveNpcMutex);
        if (g_liveNpcEnabled.load(std::memory_order_relaxed)) {
            *error = "Another live NPC AI session is already running";
            return false;
        }
        g_liveNpcInstallRequested = true;
    }
    if (!installNpcAiHost()) {
        *error = "Unable to install selective Terraria.NPC.AI native hook";
        return false;
    }
    {
        std::lock_guard<std::mutex> guard(g_liveNpcMutex);
        g_liveNpcStatus = {};
        g_liveNpcStatus.installed = true;
        g_liveNpcStatus.workerStarted = true;
        g_liveNpcStatus.npcType = npcType;
        g_liveNpcStatus.modId = modId;
        g_liveNpcStatus.phase = "armed";
        g_liveNpcPath = assemblyPath;
        g_liveNpcEnabled.store(true, std::memory_order_release);
    }
    bool createWorker = false;
    {
        std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
        ++g_liveSession;
        g_liveHasCompletion = false;
        g_liveSampleCounter.store(0, std::memory_order_relaxed);
        if (!g_liveWorkerStarted) {
            g_liveWorkerStarted = true;
            createWorker = true;
        }
    }
    if (createWorker) {
        pthread_t worker{};
        const int result = pthread_create(&worker, nullptr, [](void*) -> void* {
            liveNpcWorkerLoop();
            return nullptr;
        }, nullptr);
        if (result != 0) {
            g_liveNpcEnabled.store(false, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(g_liveDispatchMutex);
                g_liveWorkerStarted = false;
            }
            std::lock_guard<std::mutex> lock(g_liveNpcMutex);
            g_liveNpcStatus.phase = "worker-start-failed";
            *error = "Unable to launch managed NPC callback worker";
            return false;
        }
        pthread_detach(worker);
    }
    BL_INFO("csharp live NPC: async MonoVM worker enabled mod=%s type=%d max=%d interval=%d",
            modId.c_str(), npcType, kLiveNpcMaxCalls, kLiveNpcSampleInterval);
    return true;
}

bool takeNpcAttack(int slot, int type, HostNpcAttack* out) {
    if (!out || slot < 0 || static_cast<size_t>(slot) >= g_pendingAttacks.size())
        return false;
    PendingAttack& queued = g_pendingAttacks[static_cast<size_t>(slot)];
    if (!queued.ready || queued.type != type ||
        g_instances[static_cast<size_t>(slot)].generation != queued.generation)
        return false;
    *out = queued.attack;
    queued = {};
    ++g_attacksConsumed;
    return true;
}

NpcAiStatus npcAiStatus() {
    NpcAiStatus status;
    status.installed = g_installed;
    status.calls = g_calls;
    status.writes = g_writes;
    status.spawns = g_spawns;
    status.releases = g_releases;
    status.attacksQueued = g_attacksQueued;
    status.attacksConsumed = g_attacksConsumed;
    status.attacksRejected = g_attacksRejected;
    return status;
}

void requestManagedNpcBuffDispatcher() {
    g_buffDispatchRequested.store(true, std::memory_order_release);
}

bool installManagedNpcBuffDispatcher() {
    requestManagedNpcBuffDispatcher();
    if (!installNpcAiHost()) return false;
    setManagedNpcBuffGameHookReady(true);
    return true;
}

void updateManagedNpcBuffDispatcherOnGameThread() {
    if (!g_buffDispatchRequested.load(std::memory_order_acquire) ||
        managedNpcBuffEffectStatus().gameHookInstalled) return;
    installManagedNpcBuffDispatcher();
}

bool installNpcAiHost() {
    if (g_installed) return true;
    Refs& r = refs();
    if (!r.ok || (!csharpmods::hasNpcBehavior() &&
                  !g_liveNpcInstallRequested &&
                  !managedEntitySourceModsRegistered() &&
                  !g_buffDispatchRequested.load(std::memory_order_acquire))) return false;
    if (!hook::install(r.ai, hookAi, &g_origAi)) {
        BL_WARN("csharp NPC: falha no hook NPC.AI");
        return false;
    }
    if (r.checkActive && !hook::install(r.checkActive, hookCheckActive, &g_origCheckActive))
        BL_WARN("csharp NPC: hook CheckActive indisponivel");
    if (r.loot && !hook::install(r.loot, hookLoot, &g_origLoot))
        BL_WARN("csharp NPC: hook NPCLoot indisponivel");
    g_installed = true;
    BL_INFO("csharp NPC: seletivo AI nativo e/ou MonoVM C# instalado");
    return true;
}

}
