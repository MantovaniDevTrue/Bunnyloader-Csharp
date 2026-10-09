#include "experiments/csharp/ManagedProjectileHook.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/SourceLoader.h"
#include "hook/HookManager.h"

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
};

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
    const int timeLeft = runtime::field<int32_t>(projectile, game.proj.timeLeft);
    Slot& entry = g_slots[static_cast<size_t>(slotId)];
    if (entry.object != projectile || entry.owner != owner ||
        timeLeft > entry.lastTimeLeft) {
        entry = {projectile, owner, timeLeft, 0, 0};
        g_spawns.fetch_add(1, std::memory_order_relaxed);
    }
    entry.lastTimeLeft = timeLeft;
    if (entry.ticks >= 120 || g_calls.load(std::memory_order_relaxed) >= 2400) return;

    int next = 0;
    float angle = 0.f;
    if (g_buffered.load(std::memory_order_acquire)) {
        const int milliRadians = g_turns[static_cast<size_t>(entry.ticks)];
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

bool pauseManagedSourceBoltHook() {
    return g_enabled.exchange(false, std::memory_order_acq_rel);
}

ManagedProjectileHookStatus managedSourceBoltHookStatus() {
    ManagedProjectileHookStatus result;
    result.installed = g_installed.load(std::memory_order_acquire);
    result.enabled = g_enabled.load(std::memory_order_acquire);
    result.buffered = g_buffered.load(std::memory_order_acquire);
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
