#include "experiments/csharp/ManagedProjectileHook.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedRuntime.h"
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

void hkAi(Il2CppObject* projectile, const MethodInfo* method) {
    AiFn original = __atomic_load_n(&g_original, __ATOMIC_ACQUIRE);
    if (original) original(projectile, method);
    if (!g_enabled.load(std::memory_order_relaxed) || !projectile) return;

    const auto& game = runtime::game();
    if (runtime::field<int32_t>(projectile, game.proj.type) !=
        g_type.load(std::memory_order_relaxed)) return;
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
