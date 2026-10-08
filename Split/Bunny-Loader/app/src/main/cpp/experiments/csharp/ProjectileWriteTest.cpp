#include "experiments/csharp/ProjectileWriteTest.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/CSharpPrototype.h"
#include "experiments/csharp/CSharpModLoader.h"
#include "experiments/csharp/TerrariaHost.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <array>
#include <atomic>
#include <cmath>
#include <mutex>

namespace bl::csharp {

namespace {

struct Vector2 {
    float x;
    float y;
};

struct Refs {
    bool tried = false;
    bool ok = false;
    FieldInfo* projectiles = nullptr;
    const MethodInfo* getMyPlayer = nullptr;
    const MethodInfo* updatePosition = nullptr;
};

Refs& refs() {
    static Refs r;
    if (r.tried) return r;
    r.tried = true;

    using namespace il2cpp;
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    r.projectiles = main ? findField(main, "projectile") : nullptr;
    r.getMyPlayer = main ? api().class_get_method_from_name(main, "get_myPlayer", 0) : nullptr;
    r.updatePosition = projectile ? api().class_get_method_from_name(projectile, "UpdatePosition", 1) : nullptr;
    r.ok = r.projectiles && r.getMyPlayer && r.updatePosition && runtime::resolveGameRefs();

    if (!r.ok) {
        BL_WARN("csharp write: refs incompletas (projectile=%p myPlayer=%p UpdatePosition=%p)",
                (void*)r.projectiles, (void*)r.getMyPlayer, (void*)r.updatePosition);
    }
    return r;
}

Il2CppArray* projectileArray() {
    Il2CppArray* value = nullptr;
    const Refs& r = refs();
    if (r.projectiles) il2cpp::api().field_static_get_value(r.projectiles, &value);
    return value;
}

int unboxInt(Il2CppObject* boxed) {
    if (!boxed) return -1;
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject));
}

int localPlayerIndex() {
    const Refs& r = refs();
    Il2CppObject* exc = nullptr;
    Il2CppObject* boxed = il2cpp::api().runtime_invoke(r.getMyPlayer, nullptr, nullptr, &exc);
    return exc ? -1 : unboxInt(boxed);
}

Vector2 centerOf(Il2CppObject* obj) {
    const auto& g = runtime::game();
    const Vector2 p = runtime::field<Vector2>(obj, g.entity.position);
    const int width = runtime::field<int32_t>(obj, g.entity.width);
    const int height = runtime::field<int32_t>(obj, g.entity.height);
    return {p.x + width * 0.5f, p.y + height * 0.5f};
}

struct Claim {
    Il2CppObject* object = nullptr;
    int slot = -1;
    int type = -1;
    int owner = -1;
    int generation = 0;
    int targetIndex = -1;
    int writes = 0;
    int positionCalls = 0;
};

struct LastClaim {
    int slot = -1;
    int type = -1;
    int targetIndex = -1;
    int writes = 0;
    int loopCalls = 0;
};

std::mutex g_mutex;
Claim g_claim;
LastClaim g_last;
bool g_installed = false;
std::atomic<int> g_generation{0};
std::atomic<int> g_autoCalls{0};
std::atomic<int> g_autoWrites{0};
std::atomic<int> g_autoSpawns{0};
std::atomic<int> g_autoReleases{0};
int g_nextAutoGeneration = 1;

struct AutoInstance {
    Il2CppObject* object = nullptr;
    int type = -1;
    int owner = -1;
    int generation = 0;
};

std::array<AutoInstance, 4096> g_autoInstances{};

void retireAutoInstance(int slot, Il2CppObject* expected = nullptr) {
    if (slot < 0 || static_cast<size_t>(slot) >= g_autoInstances.size()) return;
    AutoInstance& entry = g_autoInstances[static_cast<size_t>(slot)];
    if (!entry.object || (expected && entry.object != expected)) return;
    const AutoInstance retired = entry;
    entry = {};
    csharpmods::releaseOwnedProjectile(retired.type, slot, retired.owner, retired.generation);
    g_autoReleases.fetch_add(1, std::memory_order_relaxed);
}

int trackAutoInstance(Il2CppObject* projectile, int slot, int type, int owner) {
    if (slot < 0 || static_cast<size_t>(slot) >= g_autoInstances.size()) return 0;
    AutoInstance& entry = g_autoInstances[static_cast<size_t>(slot)];
    if (entry.object != projectile || entry.type != type || entry.owner != owner) {
        retireAutoInstance(slot);
        if (g_nextAutoGeneration <= 0) g_nextAutoGeneration = 1;
        entry = {projectile, type, owner, g_nextAutoGeneration++};
        g_autoSpawns.fetch_add(1, std::memory_order_relaxed);
    }
    return entry.generation;
}

void retireAutoObject(Il2CppObject* projectile) {
    if (!projectile) return;
    const auto& g = runtime::game();
    const int slot = runtime::field<int32_t>(projectile, g.entity.whoAmI);
    retireAutoInstance(slot, projectile);
}

using ProjectileUpdatePositionFn = void (*)(Il2CppObject*, Vector2, const MethodInfo*);
using ProjectileKillFn = void (*)(Il2CppObject*, const MethodInfo*);
using ProjectileSetDefaultsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);

ProjectileUpdatePositionFn g_origUpdatePosition = nullptr;
ProjectileKillFn g_origKill = nullptr;
ProjectileSetDefaultsFn g_origSetDefaults = nullptr;

bool claimMatches(Il2CppObject* projectile, const Claim& claim) {
    if (!projectile || projectile != claim.object) return false;

    const auto& g = runtime::game();
    if (runtime::field<uint8_t>(projectile, g.proj.active) == 0) return false;
    if (runtime::field<int32_t>(projectile, g.proj.type) != claim.type) return false;
    if (runtime::field<int32_t>(projectile, g.proj.owner) != claim.owner) return false;
    if (runtime::field<int32_t>(projectile, g.entity.whoAmI) != claim.slot) return false;
    return true;
}

void clearClaimLocked() {
    if (g_claim.object) {
        g_last.slot = g_claim.slot;
        g_last.type = g_claim.type;
        g_last.targetIndex = g_claim.targetIndex;
        g_last.writes = g_claim.writes;
        g_last.loopCalls = g_claim.positionCalls;
    }
    g_claim = {};
}

void clearIfClaimed(Il2CppObject* projectile, const char* reason) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_claim.object != projectile) return;
    BL_INFO("csharp write: projectile[%d] liberado em %s", g_claim.slot, reason);
    clearClaimLocked();
}

bool applyWrite(Il2CppObject* projectile) {
    Claim claim;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        claim = g_claim;
    }

    if (!claim.object || projectile != claim.object) return false;
    if (!claimMatches(projectile, claim)) {
        clearIfClaimed(projectile, "validacao");
        return false;
    }

    TerrariaSnapshot snapshot = captureTerrariaSnapshot(false);
    if (!snapshot.ready) return false;

    const auto& g = runtime::game();
    const Vector2 center = centerOf(projectile);
    const Vector2 velocity = runtime::field<Vector2>(projectile, g.entity.velocity);

    HostProjectileState state{};
    state.slot = claim.slot;
    state.type = claim.type;
    state.owner = claim.owner;
    state.generation = claim.generation;
    state.targetIndex = -1;
    state.x = center.x;
    state.y = center.y;
    state.vx = velocity.x;
    state.vy = velocity.y;
    state.rotation = runtime::field<float>(projectile, g.proj.rotation);

    if (!csharpmods::dispatchProjectile(&state, snapshot.npcs.data(),
                                        static_cast<int>(snapshot.npcs.size()),
                                        snapshot.player.x, snapshot.player.y)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_claim.object == projectile && g_claim.generation == claim.generation) {
            g_claim.targetIndex = state.targetIndex;
        }
        return false;
    }

    runtime::field<Vector2>(projectile, g.entity.velocity) = {state.vx, state.vy};
    runtime::field<float>(projectile, g.proj.rotation) = state.rotation;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_claim.object == projectile && g_claim.generation == claim.generation) {
        g_claim.targetIndex = state.targetIndex;
        ++g_claim.writes;
    }
    return true;
}

bool applyAutomaticWrite(Il2CppObject* projectile) {
    if (!projectile) return false;

    const auto& g = runtime::game();
    if (runtime::field<uint8_t>(projectile, g.proj.active) == 0 ||
        runtime::field<int32_t>(projectile, g.proj.damage) <= 0 ||
        runtime::field<int32_t>(projectile, g.proj.timeLeft) <= 0) return false;

    const int type = runtime::field<int32_t>(projectile, g.proj.type);
    if (!runtime::isModProjectile(type) ||
        !csharpmods::hasOwnedProjectileBehavior(type)) return false;

    const int owner = runtime::field<int32_t>(projectile, g.proj.owner);
    const int slot = runtime::field<int32_t>(projectile, g.entity.whoAmI);
    if (slot < 0) return false;

    TerrariaSnapshot snapshot = captureTerrariaSnapshot(false);
    if (!snapshot.ready || snapshot.player.index != owner) return false;

    const Vector2 center = centerOf(projectile);
    const Vector2 velocity = runtime::field<Vector2>(projectile, g.entity.velocity);
    HostProjectileState state{};
    state.slot = slot;
    state.type = type;
    state.owner = owner;
    state.generation = trackAutoInstance(projectile, slot, type, owner);
    if (state.generation == 0) return false;
    state.targetIndex = -1;
    state.x = center.x;
    state.y = center.y;
    state.vx = velocity.x;
    state.vy = velocity.y;
    state.rotation = runtime::field<float>(projectile, g.proj.rotation);

    g_autoCalls.fetch_add(1, std::memory_order_relaxed);
    if (!csharpmods::dispatchOwnedProjectile(type, &state, snapshot.npcs.data(),
                                             static_cast<int>(snapshot.npcs.size()),
                                             snapshot.player.x, snapshot.player.y)) return false;

    if (!std::isfinite(state.vx) || !std::isfinite(state.vy) ||
        !std::isfinite(state.rotation) ||
        std::abs(state.vx) > 2048.0f || std::abs(state.vy) > 2048.0f ||
        runtime::field<uint8_t>(projectile, g.proj.active) == 0 ||
        runtime::field<int32_t>(projectile, g.proj.type) != type ||
        runtime::field<int32_t>(projectile, g.proj.owner) != owner ||
        runtime::field<int32_t>(projectile, g.entity.whoAmI) != slot) return false;

    runtime::field<Vector2>(projectile, g.entity.velocity) = {state.vx, state.vy};
    runtime::field<float>(projectile, g.proj.rotation) = state.rotation;
    g_autoWrites.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void hkUpdatePosition(Il2CppObject* self, Vector2 wetVelocity, const MethodInfo* method) {
    bool claimed = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_claim.object == self) {
            ++g_claim.positionCalls;
            claimed = true;
        }
    }

    const bool wrote = claimed ? applyWrite(self) : applyAutomaticWrite(self);
    if (wrote) {
        wetVelocity = runtime::field<Vector2>(self, runtime::game().entity.velocity);
    }
    g_origUpdatePosition(self, wetVelocity, method);
}

void hkKill(Il2CppObject* self, const MethodInfo* method) {
    retireAutoObject(self);
    clearIfClaimed(self, "Kill()");
    g_origKill(self, method);
}

void hkSetDefaults(Il2CppObject* self, int32_t type, const MethodInfo* method) {
    retireAutoObject(self);
    clearIfClaimed(self, "SetDefaults()");
    g_origSetDefaults(self, type, method);
}

ProjectileWriteStatus statusLocked() {
    ProjectileWriteStatus out;
    out.installed = g_installed;
    out.automaticCalls = g_autoCalls.load(std::memory_order_relaxed);
    out.automaticWrites = g_autoWrites.load(std::memory_order_relaxed);
    out.automaticSpawns = g_autoSpawns.load(std::memory_order_relaxed);
    out.automaticReleases = g_autoReleases.load(std::memory_order_relaxed);
    out.claimed = g_claim.object != nullptr;
    out.slot = g_claim.slot;
    out.type = g_claim.type;
    out.owner = g_claim.owner;
    out.generation = g_claim.generation;
    out.targetIndex = g_claim.targetIndex;
    out.writes = g_claim.writes;
    out.loopCalls = g_claim.positionCalls;
    out.lastSlot = g_last.slot;
    out.lastType = g_last.type;
    out.lastTargetIndex = g_last.targetIndex;
    out.lastWrites = g_last.writes;
    out.lastLoopCalls = g_last.loopCalls;
    out.valid = g_claim.object && claimMatches(g_claim.object, g_claim);
    return out;
}

}

bool installProjectileWriteTest() {
    if (g_installed) return true;
    if (!refs().ok) return false;
    if (!csharpmods::hasProjectileBehavior()) {
        BL_WARN("csharp write: nenhum mod C# externo com comportamento de projectile");
        return false;
    }

    const auto& g = runtime::game();
    if (!g.proj.kill || !g.proj.setDefaults || !refs().updatePosition) return false;

    if (!hook::install(refs().updatePosition, hkUpdatePosition, &g_origUpdatePosition)) {
        BL_WARN("csharp write: falha no hook Projectile.UpdatePosition");
        return false;
    }
    if (!hook::install(g.proj.kill, hkKill, &g_origKill)) {
        BL_WARN("csharp write: falha no hook Projectile.Kill");
        return false;
    }
    if (!hook::install(g.proj.setDefaults, hkSetDefaults, &g_origSetDefaults)) {
        BL_WARN("csharp write: falha no hook Projectile.SetDefaults");
        return false;
    }

    g_installed = true;
    BL_INFO("csharp write: host externo de projectile instalado em Projectile.UpdatePosition");
    return true;
}

ProjectileWriteStatus claimProjectile(int requestedSlot) {
    if (!installProjectileWriteTest()) return projectileWriteStatus();

    Il2CppArray* array = projectileArray();
    const int owner = localPlayerIndex();
    if (!array || owner < 0) return projectileWriteStatus();

    const auto& g = runtime::game();
    auto** data = reinterpret_cast<Il2CppObject**>(arrayData(array));

    Il2CppObject* chosen = nullptr;
    int chosenSlot = -1;

    auto eligible = [&](int slot, Il2CppObject* projectile) {
        if (!projectile) return false;
        if (runtime::field<uint8_t>(projectile, g.proj.active) == 0) return false;
        if (runtime::field<int32_t>(projectile, g.proj.owner) != owner) return false;
        if (runtime::field<int32_t>(projectile, g.proj.damage) <= 0) return false;
        if (runtime::field<int32_t>(projectile, g.proj.timeLeft) <= 0) return false;
        return runtime::field<int32_t>(projectile, g.entity.whoAmI) == slot;
    };

    if (requestedSlot >= 0 && static_cast<uintptr_t>(requestedSlot) < array->length) {
        Il2CppObject* projectile = data[requestedSlot];
        if (eligible(requestedSlot, projectile)) {
            chosen = projectile;
            chosenSlot = requestedSlot;
        }
    } else {
        for (uintptr_t i = 0; i < array->length; ++i) {
            Il2CppObject* projectile = data[i];
            if (!eligible(static_cast<int>(i), projectile)) continue;
            chosen = projectile;
            chosenSlot = static_cast<int>(i);
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        clearClaimLocked();

        if (chosen) {
            g_claim.object = chosen;
            g_claim.slot = chosenSlot;
            g_claim.type = runtime::field<int32_t>(chosen, g.proj.type);
            g_claim.owner = owner;
            g_claim.generation = g_generation.fetch_add(1, std::memory_order_relaxed) + 1;
            g_claim.targetIndex = -1;
            g_claim.writes = 0;
            g_claim.positionCalls = 0;

            BL_INFO("csharp write: claim projectile[%d] type=%d owner=%d generation=%d",
                    g_claim.slot, g_claim.type, g_claim.owner, g_claim.generation);
        }

        return statusLocked();
    }
}

void releaseProjectile() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_claim.object) BL_INFO("csharp write: release projectile[%d]", g_claim.slot);
    clearClaimLocked();
}

ProjectileWriteStatus projectileWriteStatus() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return statusLocked();
}

}
