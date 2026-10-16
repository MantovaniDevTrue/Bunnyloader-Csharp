#include "experiments/csharp/ManagedProjectileHook.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/TerrariaHost.h"
#include "experiments/csharp/SourceLoader.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

struct Vector2 {
    float x;
    float y;
};

struct Slot {
    Il2CppObject* object = nullptr;
    int owner = -1;
    int lastTimeLeft = 0;
    int ticks = 0;
    int state = 0;
    int target = -1;
    float targetX = 0.f;
    float targetY = 0.f;
};

struct NpcTargetRefs {
    FieldInfo* npcArray = nullptr;
    FieldInfo* myPlayer = nullptr;
    FieldInfo* netMode = nullptr;
    const MethodInfo* localPlayerGetter = nullptr;
    FieldInfo* gameMenu = nullptr;
    int32_t active = -1;
    int32_t friendly = -1;
    int32_t dontTakeDamage = -1;
    int32_t life = -1;
    bool ready = false;
};

NpcTargetRefs g_targets{};
SourceBoltHomingPolicy g_homingPolicy{};
std::atomic<bool> g_homing{false};
std::atomic<bool> g_singlePlayerFallback{false};
std::atomic<int> g_scans{0};
std::atomic<int> g_locks{0};
std::atomic<int> g_homingFrames{0};
std::atomic<int> g_lastTarget{-1};
int g_localPlayer = -1;

bool prepareHomingTargetRefs(std::string* error) {
    if (g_targets.ready) return true;
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* npc = il2cpp::findClass({"Terraria", "NPC", {}});
    if (!main || !npc) {
        *error = "Terraria.Main/NPC indisponiveis";
        return false;
    }
    NpcTargetRefs refs{};
    refs.npcArray = il2cpp::findField(main, "npc");
    refs.myPlayer = il2cpp::findField(main, "myPlayer");
    refs.netMode = il2cpp::findField(main, "netMode");
    refs.localPlayerGetter = il2cpp::api().class_get_method_from_name(main, "get_LocalPlayer", 0);
    refs.gameMenu = il2cpp::findField(main, "gameMenu");
    refs.active = il2cpp::fieldOffset(npc, "active");
    refs.friendly = il2cpp::fieldOffset(npc, "friendly");
    refs.life = il2cpp::fieldOffset(npc, "life");
    if (FieldInfo* damageFlag = il2cpp::findField(npc, "dontTakeDamage"))
        refs.dontTakeDamage = static_cast<int32_t>(
            il2cpp::api().field_get_offset(damageFlag));

    std::string missing;
    auto require = [&missing](bool present, const char* field) {
        if (!present) {
            if (!missing.empty()) missing += ", ";
            missing += field;
        }
    };
    require(refs.npcArray != nullptr, "Main.npc");
    require(refs.myPlayer != nullptr || refs.localPlayerGetter != nullptr || refs.netMode != nullptr,
            "Main.myPlayer, Main.LocalPlayer ou Main.netMode (fallback solo)");
    require(refs.active >= 0, "NPC.active");
    require(refs.friendly >= 0, "NPC.friendly");
    require(refs.life >= 0, "NPC.life");
    if (!missing.empty()) {
        *error = "Campos obrigatorios ausentes: " + missing;
        BL_ERROR("csharp homing: %s", error->c_str());
        return false;
    }
    refs.ready = true;
    BL_INFO("csharp homing: refs NPC ok; dontTakeDamage=%s myPlayer=%s LocalPlayer=%s netMode=%s",
            refs.dontTakeDamage >= 0 ? "presente" : "ausente",
            refs.myPlayer ? "presente" : "ausente",
            refs.localPlayerGetter ? "presente" : "ausente",
            refs.netMode ? "presente" : "ausente");
    g_targets = refs;
    return true;
}

bool refreshTarget(Slot& entry, const Vector2& position, float maxDistanceSquared) {
    ++g_scans;
    entry.target = -1;
    Il2CppArray* npcs = nullptr;
    il2cpp::api().field_static_get_value(g_targets.npcArray, &npcs);
    if (!npcs || npcs->length < 1 || npcs->length > 1024) return false;
    auto** contents = static_cast<Il2CppObject**>(arrayData(npcs));
    float bestDistance = maxDistanceSquared;
    const size_t limit = std::min(static_cast<size_t>(npcs->length), size_t{200});
    const auto& game = runtime::game();
    for (size_t i = 0; i < limit; ++i) {
        auto* npc = contents[i];
        if (!npc || !runtime::field<uint8_t>(npc, g_targets.active) ||
            runtime::field<uint8_t>(npc, g_targets.friendly) ||
            runtime::field<int32_t>(npc, g_targets.life) <= 0) continue;
        if (g_targets.dontTakeDamage >= 0 &&
            runtime::field<uint8_t>(npc, g_targets.dontTakeDamage)) continue;
        const Vector2 npcPosition = runtime::field<Vector2>(npc, game.entity.position);
        const int width = runtime::field<int32_t>(npc, game.entity.width);
        const int height = runtime::field<int32_t>(npc, game.entity.height);
        if (!std::isfinite(npcPosition.x) || !std::isfinite(npcPosition.y) ||
            width <= 0 || width > 2048 || height <= 0 || height > 2048) continue;
        const float x = npcPosition.x + static_cast<float>(width) * 0.5f;
        const float y = npcPosition.y + static_cast<float>(height) * 0.5f;
        const float dx = x - position.x;
        const float dy = y - position.y;
        const float d2 = dx * dx + dy * dy;
        if (std::isfinite(d2) && d2 < bestDistance) {
            bestDistance = d2;
            entry.target = static_cast<int>(i);
            entry.targetX = x;
            entry.targetY = y;
        }
    }
    if (entry.target >= 0) {
        ++g_locks;
        g_lastTarget.store(entry.target, std::memory_order_relaxed);
    }
    return entry.target >= 0;
}

using AiFn = void (*)(Il2CppObject*, const MethodInfo*);

std::array<Slot, 4096> g_slots{};
AiFn g_original = nullptr;
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_buffered{false};
std::array<int, 120> g_turns{};
std::atomic<int> g_type{-1};
std::atomic<int> g_calls{0};
std::atomic<int> g_writes{0};
std::atomic<int> g_spawns{0};
std::atomic<int> g_errors{0};
std::atomic<int> g_lastSlot{-1};
std::atomic<int> g_lastState{0};

std::array<Slot, 4096> g_sparkSlots{};
std::array<int, 120> g_sparkTurns{};
FieldInfo* g_sparkNetMode = nullptr;
std::atomic<int> g_sparkType{-1};
std::atomic<bool> g_sparkEnabled{false};
std::atomic<int> g_sparkSpawns{0};
std::atomic<int> g_sparkCalls{0};
std::atomic<int> g_sparkWrites{0};
std::atomic<int> g_sparkErrors{0};
std::atomic<int> g_sparkLastSlot{-1};
std::atomic<int> g_sparkLastTurn{0};

struct CurveState {
    std::string modId;
    std::string projectileName;
    int type = -1;
    std::array<int, 120> turns{};
    std::atomic<bool> enabled{false};
    std::atomic<int> spawns{0};
    std::atomic<int> callbacks{0};
    std::atomic<int> velocityWrites{0};
    std::atomic<int> errors{0};
    std::atomic<int> lastSlot{-1};
    std::atomic<int> lastTurn{0};
};

std::array<CurveState, 8> g_curveStates{};
std::array<std::atomic<int16_t>, 32768> g_curveByType{};
std::array<Slot, 4096> g_curveSlots{};
std::atomic<int> g_curveCount{0};
FieldInfo* g_curveNetMode = nullptr;

void applyManagedCurve(Il2CppObject* projectile, const runtime::GameRefs& game,
                       CurveState& policy) {
    if (!g_curveNetMode) return;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_curveNetMode, &mode);
    if (mode != 0) {
        policy.errors.fetch_add(1, std::memory_order_relaxed);
        policy.enabled.store(false, std::memory_order_release);
        return;
    }
    if (!runtime::field<uint8_t>(projectile, game.proj.active)) return;
    const int timeLeft = runtime::field<int32_t>(projectile, game.proj.timeLeft);
    if (timeLeft <= 0) return;
    const int slot = runtime::field<int32_t>(projectile, game.entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_curveSlots.size()) return;
    const int owner = runtime::field<int32_t>(projectile, game.proj.owner);
    Slot& entry = g_curveSlots[static_cast<size_t>(slot)];
    if (entry.object != projectile || entry.owner != owner ||
        entry.state != policy.type || timeLeft > entry.lastTimeLeft) {
        entry = {projectile, owner, timeLeft, 0, policy.type};
        policy.spawns.fetch_add(1, std::memory_order_relaxed);
    }
    entry.lastTimeLeft = timeLeft;
    if (entry.ticks >= 120 || policy.callbacks.load(std::memory_order_relaxed) >= 30000)
        return;
    const int angleMrad = policy.turns[static_cast<size_t>(entry.ticks++)];
    policy.callbacks.fetch_add(1, std::memory_order_relaxed);
    noteManagedLivePerfProjectileCurveUpdate();
    policy.lastSlot.store(slot, std::memory_order_relaxed);
    policy.lastTurn.store(angleMrad, std::memory_order_relaxed);
    const Vector2 velocity = runtime::field<Vector2>(projectile, game.entity.velocity);
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y)) return;
    const float squared = velocity.x * velocity.x + velocity.y * velocity.y;
    if (!std::isfinite(squared) || squared < 0.01f || squared > 4096.f) return;
    const float angle = static_cast<float>(angleMrad) * 0.001f;
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    const Vector2 rotated{
        velocity.x * cosine - velocity.y * sine,
        velocity.x * sine + velocity.y * cosine
    };
    if (!std::isfinite(rotated.x) || !std::isfinite(rotated.y)) return;
    runtime::field<Vector2>(projectile, game.entity.velocity) = rotated;
    runtime::field<float>(projectile, game.proj.rotation) =
        std::atan2(rotated.y, rotated.x);
    policy.velocityWrites.fetch_add(1, std::memory_order_relaxed);
    noteManagedLivePerfProjectileVelocityWrite();
}

void applySparkAi(Il2CppObject* projectile, const runtime::GameRefs& game) {
    if (!g_sparkNetMode) return;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_sparkNetMode, &mode);
    if (mode != 0) {
        g_sparkErrors.fetch_add(1, std::memory_order_relaxed);
        g_sparkEnabled.store(false, std::memory_order_release);
        return;
    }
    if (!runtime::field<uint8_t>(projectile, game.proj.active)) return;
    const int timeLeft = runtime::field<int32_t>(projectile, game.proj.timeLeft);
    if (timeLeft <= 0) return;
    const int slot = runtime::field<int32_t>(projectile, game.entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_sparkSlots.size()) return;
    const int owner = runtime::field<int32_t>(projectile, game.proj.owner);
    Slot& entry = g_sparkSlots[static_cast<size_t>(slot)];
    if (entry.object != projectile || entry.owner != owner ||
        timeLeft > entry.lastTimeLeft) {
        entry = {projectile, owner, timeLeft, 0, 0};
        g_sparkSpawns.fetch_add(1, std::memory_order_relaxed);
    }
    entry.lastTimeLeft = timeLeft;
    if (entry.ticks >= 120 || g_sparkCalls.load(std::memory_order_relaxed) >= 30000) return;
    const int turn = g_sparkTurns[static_cast<size_t>(entry.ticks)];
    ++entry.ticks;
    g_sparkCalls.fetch_add(1, std::memory_order_relaxed);
    g_sparkLastSlot.store(slot, std::memory_order_relaxed);
    g_sparkLastTurn.store(turn, std::memory_order_relaxed);

    Vector2 velocity = runtime::field<Vector2>(projectile, game.entity.velocity);
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y)) return;
    const float speedSquared = velocity.x * velocity.x + velocity.y * velocity.y;
    if (!std::isfinite(speedSquared) || speedSquared < 0.01f ||
        speedSquared > 4096.f) return;
    const float angle = static_cast<float>(turn) * 0.001f;
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    const Vector2 rotated{
        velocity.x * cosine - velocity.y * sine,
        velocity.x * sine + velocity.y * cosine
    };
    if (!std::isfinite(rotated.x) || !std::isfinite(rotated.y)) return;
    runtime::field<Vector2>(projectile, game.entity.velocity) = rotated;
    runtime::field<float>(projectile, game.proj.rotation) =
        std::atan2(rotated.y, rotated.x);
    g_sparkWrites.fetch_add(1, std::memory_order_relaxed);
}

struct ManagedEntityProjectileSlot {
    Il2CppObject* object = nullptr;
    int type = -1;
    int owner = -1;
    int lastTimeLeft = 0;
    int generation = 0;
    bool killQueued = false;
};
std::array<ManagedEntityProjectileSlot, 4096> g_managedEntityProjectileSlots{};
int g_managedEntityProjectileGeneration = 1;
FieldInfo* g_managedEntityNetMode = nullptr;

void runManagedEntityProjectile(Il2CppObject* projectile,
                                const runtime::GameRefs& game, int type,
                                bool applyVelocity) {
    if (!hasManagedEntityProjectileType(type) ||
        !runtime::field<uint8_t>(projectile, game.proj.active)) return;
    if (!g_managedEntityNetMode) {
        auto* main = il2cpp::findClass({"Terraria", "Main", {}});
        g_managedEntityNetMode = main ? il2cpp::findField(main, "netMode") : nullptr;
    }
    if (!g_managedEntityNetMode) return;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_managedEntityNetMode, &mode);
    if (mode != 0) return;
    const int slot = runtime::field<int>(projectile, game.entity.whoAmI);
    const int timeLeft = runtime::field<int>(projectile, game.proj.timeLeft);
    if (slot < 0 || slot >= static_cast<int>(g_managedEntityProjectileSlots.size()) ||
        timeLeft <= 0) return;
    const int owner = runtime::field<int>(projectile, game.proj.owner);
    ManagedEntityProjectileSlot& state =
        g_managedEntityProjectileSlots[static_cast<size_t>(slot)];
    const bool firstObserved = state.object != projectile || state.type != type ||
        state.owner != owner || timeLeft > state.lastTimeLeft;
    if (firstObserved) {
        if (g_managedEntityProjectileGeneration <= 0)
            g_managedEntityProjectileGeneration = 1;
        state = {projectile, type, owner, timeLeft, g_managedEntityProjectileGeneration++, false};
    }
    state.lastTimeLeft = timeLeft;
    const TerrariaSnapshot target = captureTerrariaSnapshot(false, false);
    if (!target.ready || target.player.life <= 0) return;
    const Vector2 position = runtime::field<Vector2>(projectile, game.entity.position);
    const Vector2 before = runtime::field<Vector2>(projectile, game.entity.velocity);
    const int width = runtime::field<int>(projectile, game.entity.width);
    const int height = runtime::field<int>(projectile, game.entity.height);
    ManagedEntityVelocity next{};
    if (!tickManagedEntityProjectile(type, slot, state.generation, owner,
        reinterpret_cast<uintptr_t>(projectile),
        position.x + width * 0.5f, position.y + height * 0.5f,
        before.x, before.y, target.player.x, target.player.y, &next,
        firstObserved)) return;
    if (!applyVelocity) return;
    if (!runtime::field<uint8_t>(projectile, game.proj.active) ||
        runtime::field<int>(projectile, game.proj.type) != type ||
        runtime::field<int>(projectile, game.entity.whoAmI) != slot ||
        state.object != projectile || state.owner != owner) return;
    runtime::field<Vector2>(projectile, game.entity.velocity) = {next.x, next.y};
    const float squared = next.x * next.x + next.y * next.y;
    if (squared > 0.001f)
        runtime::field<float>(projectile, game.proj.rotation) =
            std::atan2(next.y, next.x);
}

void hkAi(Il2CppObject* projectile, const MethodInfo* method) {
    if (projectile && managedEntitySourceModsRegistered()) {
        const auto& game = runtime::game();
        const int type = runtime::field<int32_t>(projectile, game.proj.type);
        if (hasManagedEntityProjectileType(type) &&
            runtime::field<uint8_t>(projectile, game.proj.active)) {
            const int slot = runtime::field<int>(projectile, game.entity.whoAmI);
            if (slot >= 0 &&
                static_cast<size_t>(slot) < g_managedEntityProjectileSlots.size()) {
                const int owner = runtime::field<int>(projectile, game.proj.owner);
                const int remaining = runtime::field<int>(projectile, game.proj.timeLeft);
                const auto& known = g_managedEntityProjectileSlots[static_cast<size_t>(slot)];
                if (remaining > 0 &&
                    (known.object != projectile || known.type != type ||
                     known.owner != owner || remaining > known.lastTimeLeft))
                    runManagedEntityProjectile(projectile, game, type, false);
            }
        }
    }
    AiFn original = __atomic_load_n(&g_original, __ATOMIC_ACQUIRE);
    if (original) original(projectile, method);
    if (!projectile) return;

    const auto& game = runtime::game();
    const int projectileType = runtime::field<int32_t>(projectile, game.proj.type);
    bool coveredByCurve = false;
    if (projectileType >= 0 &&
        static_cast<size_t>(projectileType) < g_curveByType.size()) {
        const int curveIndex = g_curveByType[static_cast<size_t>(projectileType)]
            .load(std::memory_order_acquire) - 1;
        if (curveIndex >= 0 && curveIndex < g_curveCount.load(std::memory_order_acquire))
            coveredByCurve = g_curveStates[static_cast<size_t>(curveIndex)]
                .enabled.load(std::memory_order_acquire);
    }
    const bool nativeMotion = coveredByCurve ||
        (g_sparkEnabled.load(std::memory_order_acquire) &&
         projectileType == g_sparkType.load(std::memory_order_relaxed)) ||
        (g_enabled.load(std::memory_order_acquire) &&
         projectileType == g_type.load(std::memory_order_relaxed));
    runManagedEntityProjectile(projectile, game, projectileType, !nativeMotion);
    if (projectileType >= 0 &&
        static_cast<size_t>(projectileType) < g_curveByType.size()) {
        const int slot = g_curveByType[static_cast<size_t>(projectileType)]
            .load(std::memory_order_acquire) - 1;
        if (slot >= 0 && slot < g_curveCount.load(std::memory_order_acquire)) {
            CurveState& curve = g_curveStates[static_cast<size_t>(slot)];
            if (curve.enabled.load(std::memory_order_acquire)) {
                applyManagedCurve(projectile, game, curve);
                return;
            }
        }
    }
    if (g_sparkEnabled.load(std::memory_order_relaxed) &&
        projectileType == g_sparkType.load(std::memory_order_relaxed)) {
        applySparkAi(projectile, game);
        return;
    }
    if (!g_enabled.load(std::memory_order_relaxed) ||
        projectileType != g_type.load(std::memory_order_relaxed)) return;
    if (!runtime::field<uint8_t>(projectile, game.proj.active) ||
        runtime::field<int32_t>(projectile, game.proj.timeLeft) <= 0) return;

    const int slotId = runtime::field<int32_t>(projectile, game.entity.whoAmI);
    if (slotId < 0 || static_cast<size_t>(slotId) >= g_slots.size()) return;
    const int owner = runtime::field<int32_t>(projectile, game.proj.owner);
    if (g_homing.load(std::memory_order_relaxed)) {
        if (g_singlePlayerFallback.load(std::memory_order_relaxed)) {
            if (owner < 0 || owner >= 255) return;
            int mode = -1;
            il2cpp::api().field_static_get_value(g_targets.netMode, &mode);
            if (mode != 0) {
                g_enabled.store(false, std::memory_order_release);
                g_errors.fetch_add(1, std::memory_order_relaxed);
                BL_ERROR("csharp homing: modo solo encerrado; hook desativado");
                return;
            }
        } else if (owner != g_localPlayer) {
            return;
        }
    }
    const int timeLeft = runtime::field<int32_t>(projectile, game.proj.timeLeft);
    Slot& entry = g_slots[static_cast<size_t>(slotId)];
    if (entry.object != projectile || entry.owner != owner ||
        timeLeft > entry.lastTimeLeft) {
        entry = {projectile, owner, timeLeft, 0, 0};
        g_spawns.fetch_add(1, std::memory_order_relaxed);
    }
    entry.lastTimeLeft = timeLeft;
    if (entry.ticks >= 120 || g_calls.load(std::memory_order_relaxed) >= 30000) return;

    int next = 0;
    float angle = 0.f;
    if (g_buffered.load(std::memory_order_acquire)) {
        int milliRadians = g_turns[static_cast<size_t>(entry.ticks)];
        if (g_homing.load(std::memory_order_relaxed)) {
            const Vector2 projPos = runtime::field<Vector2>(projectile, game.entity.position);
            const int width = runtime::field<int32_t>(projectile, game.entity.width);
            const int height = runtime::field<int32_t>(projectile, game.entity.height);
            const Vector2 center{
                projPos.x + static_cast<float>(width) * 0.5f,
                projPos.y + static_cast<float>(height) * 0.5f
            };
            if (std::isfinite(center.x) && std::isfinite(center.y)) {
                if (entry.ticks % g_homingPolicy.retargetTicks == 0)
                    refreshTarget(entry, center,
                        static_cast<float>(g_homingPolicy.radius * g_homingPolicy.radius));
                if (entry.target >= 0) {
                    const Vector2 velocity = runtime::field<Vector2>(projectile, game.entity.velocity);
                    const float dx = entry.targetX - center.x;
                    const float dy = entry.targetY - center.y;
                    if (std::isfinite(velocity.x) && std::isfinite(velocity.y) &&
                        std::isfinite(dx) && std::isfinite(dy) &&
                        dx * dx + dy * dy > 25.f) {
                        const float error = std::atan2(
                            velocity.x * dy - velocity.y * dx,
                            velocity.x * dx + velocity.y * dy);
                        const int lookup = std::clamp(
                            static_cast<int>(std::lround(error * (64.f / 3.14159265358979323846f))) + 64,
                            0, 128);
                        milliRadians = g_homingPolicy.turnsMilliRadians[static_cast<size_t>(lookup)];
                        ++g_homingFrames;
                    }
                }
            }
        }
        next = entry.state + milliRadians;
        angle = milliRadians * 0.001f;
    } else {
        if (!invokeManagedProjectileAI(entry.ticks, entry.state, &next)) {
            g_errors.fetch_add(1, std::memory_order_relaxed);
            g_enabled.store(false, std::memory_order_release);
            BL_ERROR("csharp AI: SourceBolt desativado apos falha gerenciada");
            return;
        }
        angle = static_cast<float>(next - entry.state) * 0.006f;
    }

    entry.state = next;
    ++entry.ticks;
    g_calls.fetch_add(1, std::memory_order_relaxed);
    g_lastSlot.store(slotId, std::memory_order_relaxed);
    g_lastState.store(next, std::memory_order_relaxed);

    Vector2 velocity = runtime::field<Vector2>(projectile, game.entity.velocity);
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y)) return;
    const float speedSquared = velocity.x * velocity.x + velocity.y * velocity.y;
    if (!std::isfinite(speedSquared) || speedSquared < 0.01f ||
        speedSquared > 4096.0f) return;

    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    const Vector2 nextVelocity{
        velocity.x * cosine - velocity.y * sine,
        velocity.x * sine + velocity.y * cosine
    };
    if (!std::isfinite(nextVelocity.x) || !std::isfinite(nextVelocity.y)) return;

    runtime::field<Vector2>(projectile, game.entity.velocity) = nextVelocity;
    runtime::field<float>(projectile, game.proj.rotation) =
        std::atan2(nextVelocity.y, nextVelocity.x);
    g_writes.fetch_add(1, std::memory_order_relaxed);
}

}

bool queueManagedSourceProjectileCombat(int eventKind, Il2CppObject* projectile,
                                        int npcIndex, int damage) {
    if (!projectile || (eventKind != 4 && eventKind != 5) ||
        !runtime::resolveGameRefs()) return false;
    const auto& game = runtime::game();
    const int type = runtime::field<int>(projectile, game.proj.type);
    if (!hasManagedEntityProjectileType(type)) return false;
    if (!g_managedEntityNetMode) return false;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_managedEntityNetMode, &mode);
    if (mode != 0) return false;
    const int slot = runtime::field<int>(projectile, game.entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_managedEntityProjectileSlots.size())
        return false;
    ManagedEntityProjectileSlot& state =
        g_managedEntityProjectileSlots[static_cast<size_t>(slot)];
    const int owner = runtime::field<int>(projectile, game.proj.owner);
    if (state.object != projectile || state.type != type || state.owner != owner ||
        state.generation <= 0 || state.killQueued) return false;
    if (eventKind == 5) state.killQueued = true;
    return queueManagedEntityCombatEvent(eventKind, type, slot, state.generation,
        owner, reinterpret_cast<uintptr_t>(projectile),
        std::clamp(damage, 0, 1000000), npcIndex);
}

void resetManagedSourceProjectileSlots() {
    g_managedEntityProjectileSlots.fill({});
}

bool installManagedEntityProjectileHook() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (!runtime::resolveGameRefs() || !runtime::game().proj.aiMethod) return false;
    if (!hook::install(runtime::game().proj.aiMethod, hkAi, &g_original)) return false;
    g_installed.store(true, std::memory_order_release);
    BL_INFO("csharp source projectile: seletivo AI MonoVM instalado");
    return true;
}

bool enableManagedProjectileCurve(const std::string& modId,
                                  const std::string& projectileName,
                                  const std::array<int, 120>& turns,
                                  std::string* error, bool allowInMenu) {
    if (!error) return false;
    error->clear();
    if (modId.empty() || modId.size() > 128 ||
        projectileName.empty() || projectileName.size() > 72) {
        *error = "ID do mod ou nome do projetil invalido";
        return false;
    }
    for (char c : projectileName) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            *error = "Nome de projetil C# contem caracteres invalidos";
            return false;
        }
    }
    for (int angle : turns) {
        if (angle < -80 || angle > 80) {
            *error = "Curva C# fora do limite de 80 mrad";
            return false;
        }
    }
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Tabelas Terraria indisponiveis";
        return false;
    }
    bool registered = false;
    for (const auto& mod : sourceModStatuses()) {
        if (mod.id == modId && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains(projectileName)) {
            registered = true;
            break;
        }
    }
    const int type = runtime::modProjectileTypeByName(modId, projectileName);
    if (!registered || type < 0 || type >= runtime::projectileTypeCount() ||
        static_cast<size_t>(type) >= g_curveByType.size()) {
        *error = "Projetil C# nao registrado ou fora dos limites";
        return false;
    }
    if (projectileName == "SourceBolt" && g_enabled.load(std::memory_order_acquire) &&
        type == g_type.load(std::memory_order_relaxed)) {
        *error = "Desative a IA de SourceBolt antes de usar outra curva no mesmo projetil";
        return false;
    }
    if (projectileName == "SourceSpark" && g_sparkEnabled.load(std::memory_order_acquire) &&
        type == g_sparkType.load(std::memory_order_relaxed)) {
        *error = "Desative sourceEnableSparkAI antes de usar a API generica";
        return false;
    }
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!main || !runtime::game().proj.aiMethod) {
        *error = "Main ou Projectile.AI indisponivel";
        return false;
    }
    FieldInfo* modeField = il2cpp::findField(main, "netMode");
    FieldInfo* menuField = il2cpp::findField(main, "gameMenu");
    if (!modeField) {
        *error = "Main.netMode indisponivel";
        return false;
    }
    int mode = -1;
    il2cpp::api().field_static_get_value(modeField, &mode);
    if (mode != 0) {
        *error = "IA C# exclusiva do singleplayer nesta versao";
        return false;
    }
    if (menuField) {
        uint8_t menu = 0;
        il2cpp::api().field_static_get_value(menuField, &menu);
        if (menu && !allowInMenu) {
            *error = "Entre no mundo antes de ativar a IA C#";
            return false;
        }
    }
    if (!g_installed.load(std::memory_order_acquire)) {
        if (!hook::install(runtime::game().proj.aiMethod, hkAi, &g_original)) {
            *error = "Falha ao instalar hook Projectile.AI";
            return false;
        }
        g_installed.store(true, std::memory_order_release);
    }

    int index = -1;
    const int count = g_curveCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (g_curveStates[static_cast<size_t>(i)].modId == modId &&
            g_curveStates[static_cast<size_t>(i)].projectileName == projectileName) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        if (count >= static_cast<int>(g_curveStates.size())) {
            *error = "Limite de oito politicas C# por sessao atingido";
            return false;
        }
        index = count;
    }
    CurveState& curve = g_curveStates[static_cast<size_t>(index)];
    curve.enabled.store(false, std::memory_order_release);
    curve.modId = modId;
    curve.projectileName = projectileName;
    curve.type = type;
    curve.turns = turns;
    curve.spawns.store(0, std::memory_order_relaxed);
    curve.callbacks.store(0, std::memory_order_relaxed);
    curve.velocityWrites.store(0, std::memory_order_relaxed);
    curve.errors.store(0, std::memory_order_relaxed);
    curve.lastSlot.store(-1, std::memory_order_relaxed);
    curve.lastTurn.store(0, std::memory_order_relaxed);
    g_curveNetMode = modeField;
    if (index == count)
        g_curveCount.store(count + 1, std::memory_order_release);
    curve.enabled.store(true, std::memory_order_release);
    g_curveByType[static_cast<size_t>(type)].store(static_cast<int16_t>(index + 1),
                                                  std::memory_order_release);
    BL_INFO("csharp projectile curve: %s/%s type=%d samples=120",
            modId.c_str(), projectileName.c_str(), type);
    return true;
}

bool pauseManagedProjectileCurve(const std::string& modId,
                                 const std::string& projectileName) {
    const int count = g_curveCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        auto& policy = g_curveStates[static_cast<size_t>(i)];
        if (policy.modId == modId && policy.projectileName == projectileName)
            return policy.enabled.exchange(false, std::memory_order_acq_rel);
    }
    return false;
}

ManagedProjectileCurveStatus managedProjectileCurveStatus(
    const std::string& modId, const std::string& projectileName) {
    ManagedProjectileCurveStatus status;
    status.installed = g_installed.load(std::memory_order_acquire);
    const int count = g_curveCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        const auto& policy = g_curveStates[static_cast<size_t>(i)];
        if (policy.modId != modId || policy.projectileName != projectileName) continue;
        status.registered = true;
        status.enabled = policy.enabled.load(std::memory_order_acquire);
        status.type = policy.type;
        status.spawns = policy.spawns.load(std::memory_order_relaxed);
        status.callbacks = policy.callbacks.load(std::memory_order_relaxed);
        status.velocityWrites = policy.velocityWrites.load(std::memory_order_relaxed);
        status.errors = policy.errors.load(std::memory_order_relaxed);
        status.lastSlot = policy.lastSlot.load(std::memory_order_relaxed);
        status.lastTurn = policy.lastTurn.load(std::memory_order_relaxed);
        break;
    }
    return status;
}

bool enableSourceSparkAi(const std::array<int, 120>& turnsMilliRadians, std::string* error) {
    if (!error) return false;
    error->clear();
    int nonzero = 0;
    for (const int turn : turnsMilliRadians) {
        if (turn < -60 || turn > 60) {
            *error = "SourceSpark C# turn fora do limite de 60 mrad";
            return false;
        }
        if (turn != 0) ++nonzero;
    }
    if (nonzero < 80) {
        *error = "SourceSpark AI precisa de movimento C# independente";
        return false;
    }
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Tabelas Terraria indisponiveis";
        return false;
    }
    constexpr const char* uid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    bool applied = false;
    for (const auto& mod : sourceModStatuses()) {
        if (mod.id == uid && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains("SourceSpark")) {
            applied = true;
            break;
        }
    }
    const int type = runtime::modProjectileTypeByName(uid, "SourceSpark");
    if (type >= 0 && static_cast<size_t>(type) < g_curveByType.size()) {
        const int index = g_curveByType[static_cast<size_t>(type)].load(
            std::memory_order_acquire) - 1;
        if (index >= 0 && index < g_curveCount.load(std::memory_order_acquire) &&
            g_curveStates[static_cast<size_t>(index)].enabled.load(
                std::memory_order_acquire)) {
            *error = "Desative a curva generica do SourceSpark antes da IA antiga";
            return false;
        }
    }
    if (!applied || type < 0 || type >= runtime::projectileTypeCount()) {
        *error = "SourceSpark nao registrado ou SetDefaults nao aplicado";
        return false;
    }
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!main || !runtime::game().proj.aiMethod) {
        *error = "Terraria.Main ou Projectile.AI indisponivel";
        return false;
    }
    FieldInfo* modeField = il2cpp::findField(main, "netMode");
    FieldInfo* menuField = il2cpp::findField(main, "gameMenu");
    if (!modeField) {
        *error = "Main.netMode ausente; SourceSpark AI precisa do singleplayer";
        return false;
    }
    int mode = -1;
    il2cpp::api().field_static_get_value(modeField, &mode);
    if (mode != 0) {
        *error = "SourceSpark AI exclusivo do singleplayer";
        return false;
    }
    if (menuField) {
        uint8_t inMenu = 0;
        il2cpp::api().field_static_get_value(menuField, &inMenu);
        if (inMenu) {
            *error = "Entre no mundo antes de ativar SourceSpark AI";
            return false;
        }
    }
    g_sparkEnabled.store(false, std::memory_order_release);
    if (!g_installed.load(std::memory_order_acquire)) {
        if (!hook::install(runtime::game().proj.aiMethod, hkAi, &g_original)) {
            *error = "Falha ao instalar hook Projectile.AI para SourceSpark";
            return false;
        }
        g_installed.store(true, std::memory_order_release);
    }
    for (auto& slot : g_sparkSlots) slot = {};
    g_sparkTurns = turnsMilliRadians;
    g_sparkNetMode = modeField;
    g_sparkType.store(type, std::memory_order_relaxed);
    g_sparkSpawns.store(0, std::memory_order_relaxed);
    g_sparkCalls.store(0, std::memory_order_relaxed);
    g_sparkWrites.store(0, std::memory_order_relaxed);
    g_sparkErrors.store(0, std::memory_order_relaxed);
    g_sparkLastSlot.store(-1, std::memory_order_relaxed);
    g_sparkLastTurn.store(0, std::memory_order_relaxed);
    g_sparkEnabled.store(true, std::memory_order_release);
    BL_INFO("csharp SourceSpark AI: tipo %d, buffer 120, independente do SourceBolt", type);
    return true;
}

bool pauseSourceSparkAi() {
    return g_sparkEnabled.exchange(false, std::memory_order_acq_rel);
}

SourceSparkAiStatus sourceSparkAiStatus() {
    SourceSparkAiStatus status;
    status.installed = g_installed.load(std::memory_order_acquire);
    status.enabled = g_sparkEnabled.load(std::memory_order_acquire);
    status.type = g_sparkType.load(std::memory_order_relaxed);
    status.samples = status.enabled ? 120 : 0;
    status.spawns = g_sparkSpawns.load(std::memory_order_relaxed);
    status.callbacks = g_sparkCalls.load(std::memory_order_relaxed);
    status.velocityWrites = g_sparkWrites.load(std::memory_order_relaxed);
    status.errors = g_sparkErrors.load(std::memory_order_relaxed);
    status.lastSlot = g_sparkLastSlot.load(std::memory_order_relaxed);
    status.lastTurn = g_sparkLastTurn.load(std::memory_order_relaxed);
    return status;
}

bool armSourceBoltHook(std::string* error, const std::array<int, 120>* turns) {
    if (!error) return false;
    error->clear();
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Tabelas de projeteis do Terraria ainda indisponiveis";
        return false;
    }
    constexpr const char* sourceUid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    int type = -1;
    bool loaded = false;
    for (const auto& source : sourceModStatuses()) {
        if (source.id != sourceUid || !source.loaded || source.projectiles <= 0) continue;
        loaded = true;
        type = runtime::modProjectileTypeByName(source.id, "SourceBolt");
        break;
    }
    if (type < 0) {
        *error = loaded
            ? "SourceBolt nao encontrado no registro do Source C# Test"
            : "Source C# Test 0.1.0 nao carregado como mod fonte C#";
        return false;
    }
    if (type >= runtime::projectileTypeCount()) {
        *error = "SourceBolt registrado, mas ainda nao instalado nas tabelas do jogo";
        return false;
    }
    if (static_cast<size_t>(type) < g_curveByType.size()) {
        const int index = g_curveByType[static_cast<size_t>(type)].load(
            std::memory_order_acquire) - 1;
        if (index >= 0 && index < g_curveCount.load(std::memory_order_acquire) &&
            g_curveStates[static_cast<size_t>(index)].enabled.load(
                std::memory_order_acquire)) {
            *error = "Desative a curva generica de SourceBolt antes do homing";
            return false;
        }
    }
    BL_INFO("csharp AI: SourceBolt localizado pelo UUID do mod, tipo %d", type);
    const auto& game = runtime::game();
    if (!game.proj.aiMethod) {
        *error = "Projectile.AI indisponivel nesta versao do Terraria";
        return false;
    }

    g_enabled.store(false, std::memory_order_release);
    if (!g_installed.load(std::memory_order_acquire)) {
        if (!hook::install(game.proj.aiMethod, hkAi, &g_original)) {
            *error = "Falha ao instalar hook de Projectile.AI";
            return false;
        }
        g_installed.store(true, std::memory_order_release);
    }
    for (auto& slot : g_slots) slot = {};
    g_homing.store(false, std::memory_order_release);
    g_singlePlayerFallback.store(false, std::memory_order_release);
    g_scans.store(0, std::memory_order_relaxed);
    g_locks.store(0, std::memory_order_relaxed);
    g_homingFrames.store(0, std::memory_order_relaxed);
    g_lastTarget.store(-1, std::memory_order_relaxed);
    g_calls.store(0, std::memory_order_relaxed);
    g_writes.store(0, std::memory_order_relaxed);
    g_spawns.store(0, std::memory_order_relaxed);
    g_errors.store(0, std::memory_order_relaxed);
    g_lastSlot.store(-1, std::memory_order_relaxed);
    g_lastState.store(0, std::memory_order_relaxed);
    g_type.store(type, std::memory_order_relaxed);
    if (turns) g_turns = *turns;
    g_buffered.store(turns != nullptr, std::memory_order_release);
    g_enabled.store(true, std::memory_order_release);
    BL_INFO("csharp AI: SourceBolt hook ativado para tipo %d (buffer=%d)",
            type, turns ? 1 : 0);
    return true;
}

bool enableManagedSourceBoltHook(std::string* error) {
    if (!error) return false;
    if (!managedRuntimeStatus().domainInitialized) {
        *error = "Primeiro executar managedGameProbe no processo do jogo";
        return false;
    }
    return armSourceBoltHook(error, nullptr);
}

bool enableBufferedSourceBoltHook(
    const std::array<int, 120>& turnsMilliRadians, std::string* error) {
    if (!error) return false;
    for (int turn : turnsMilliRadians) {
        if (turn < -80 || turn > 80) {
            *error = "Curva de IA C# excedeu 80 milirradianos por tick";
            return false;
        }
    }
    return armSourceBoltHook(error, &turnsMilliRadians);
}

bool enableHomingSourceBoltHook(const SourceBoltHomingPolicy& policy,
                                 const std::array<int, 120>& fallbackTurns,
                                 std::string* error) {
    if (!error) return false;
    error->clear();
    if (policy.radius < 64 || policy.radius > 720 ||
        policy.retargetTicks < 4 || policy.retargetTicks > 24) {
        *error = "HomingRange/RetargetInterval fora dos limites";
        return false;
    }
    for (int turn : policy.turnsMilliRadians)
        if (turn < -80 || turn > 80) {
            *error = "Curva de perseguição excede limite de 80 mrad";
            return false;
        }
    for (int turn : fallbackTurns)
        if (turn < -80 || turn > 80) {
            *error = "Curva de movimento excede limite de 80 mrad";
            return false;
        }
    if (!prepareHomingTargetRefs(error)) return false;
    if (g_targets.gameMenu) {
        uint8_t inMenu = 0;
        il2cpp::api().field_static_get_value(g_targets.gameMenu, &inMenu);
        if (inMenu) {
            *error = "Entre em um mundo antes de ativar perseguicao C#";
            return false;
        }
    }
    int localPlayer = -1;
    bool singlePlayerFallback = false;
    if (g_targets.myPlayer)
        il2cpp::api().field_static_get_value(g_targets.myPlayer, &localPlayer);
    if ((localPlayer < 0 || localPlayer >= 255) && g_targets.localPlayerGetter) {
        if (runtime::resolveGameRefs()) {
            Il2CppObject* exception = nullptr;
            Il2CppObject* player = il2cpp::api().runtime_invoke(
                g_targets.localPlayerGetter, nullptr, nullptr, &exception);
            if (!exception && player)
                localPlayer = runtime::field<int32_t>(player, runtime::game().entity.whoAmI);
        }
    }
    if (localPlayer < 0 || localPlayer >= 255) {
        if (!g_targets.netMode) {
            *error = "Main.myPlayer/LocalPlayer invalidos e Main.netMode indisponivel";
            return false;
        }
        int mode = -1;
        il2cpp::api().field_static_get_value(g_targets.netMode, &mode);
        if (mode != 0) {
            *error = "Main.myPlayer ausente: modo alternativo permitido apenas no mundo solo";
            return false;
        }
        singlePlayerFallback = true;
    }
    if (!armSourceBoltHook(error, &fallbackTurns)) return false;
    g_localPlayer = localPlayer;
    g_homingPolicy = policy;
    g_singlePlayerFallback.store(singlePlayerFallback, std::memory_order_release);
    g_homing.store(true, std::memory_order_release);
    BL_INFO("csharp AI: homing ativo radius=%d retarget=%d player=%d soloFallback=%d",
            policy.radius, policy.retargetTicks, localPlayer,
            singlePlayerFallback ? 1 : 0);
    return true;
}

bool pauseManagedSourceBoltHook() {
    return g_enabled.exchange(false, std::memory_order_acq_rel);
}

ManagedProjectileHookStatus managedSourceBoltHookStatus() {
    ManagedProjectileHookStatus result;
    result.installed = g_installed.load(std::memory_order_acquire);
    result.enabled = g_enabled.load(std::memory_order_acquire);
    result.buffered = g_buffered.load(std::memory_order_acquire);
    result.homing = g_homing.load(std::memory_order_acquire);
    result.homingScans = g_scans.load(std::memory_order_relaxed);
    result.homingLocks = g_locks.load(std::memory_order_relaxed);
    result.homingFrames = g_homingFrames.load(std::memory_order_relaxed);
    result.lastTarget = g_lastTarget.load(std::memory_order_relaxed);
    const auto hit = sourceBoltHitStatus();
    result.hitInstalled = hit.installed;
    result.hitEnabled = hit.enabled;
    result.hitEvents = hit.events;
    result.buffsApplied = hit.applied;
    result.buffErrors = hit.errors;
    result.lastHitNpc = hit.lastNpc;
    result.hitBuffId = hit.buffId;
    result.bufferSamples = result.buffered ? 120 : 0;
    result.type = g_type.load(std::memory_order_relaxed);
    result.callbacks = g_calls.load(std::memory_order_relaxed);
    result.velocityWrites = g_writes.load(std::memory_order_relaxed);
    result.spawns = g_spawns.load(std::memory_order_relaxed);
    result.errors = g_errors.load(std::memory_order_relaxed);
    result.lastSlot = g_lastSlot.load(std::memory_order_relaxed);
    result.lastState = g_lastState.load(std::memory_order_relaxed);
    return result;
}

}
