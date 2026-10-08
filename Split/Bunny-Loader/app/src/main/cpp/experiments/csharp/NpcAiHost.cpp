#include "experiments/csharp/NpcAiHost.h"

#include "content/common/GameRefs.h"
#include "content/npcs/ModNpcs.h"
#include "core/Log.h"
#include "experiments/csharp/CSharpModLoader.h"
#include "experiments/csharp/TerrariaHost.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

struct Vector2 { float x, y; };

struct Refs {
    bool attempted = false;
    bool ok = false;
    FieldInfo* netMode = nullptr;
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
    if (r.attempted) return r;
    r.attempted = true;
    using namespace il2cpp;
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    if (!npc || !main || !runtime::resolveGameRefs()) return r;

    r.netMode = findField(main, "netMode");
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

void retire(int slot, Il2CppObject* expected = nullptr) {
    if (slot < 0 || static_cast<size_t>(slot) >= g_instances.size()) return;
    Instance& entry = g_instances[static_cast<size_t>(slot)];
    if (!entry.object || (expected && entry.object != expected)) return;
    const Instance previous = entry;
    entry = {};
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
    if (netMode != 0) return;

    const int type = runtime::field<int>(npc, r.type);
    if (!runtime::isModNpc(type) || !csharpmods::hasOwnedNpcBehavior(type)) return;
    if (!runtime::field<uint8_t>(npc, r.active) ||
        runtime::field<int>(npc, r.life) <= 0) {
        retireObject(npc);
        return;
    }

    const int slot = runtime::field<int>(npc, runtime::game().entity.whoAmI);
    if (slot < 0 || static_cast<size_t>(slot) >= g_instances.size()) return;
    TerrariaSnapshot snapshot = captureTerrariaSnapshot(false);
    if (!snapshot.ready || snapshot.player.life <= 0) return;

    Instance& entry = g_instances[static_cast<size_t>(slot)];
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
}

void hookAi(Il2CppObject* npc, const MethodInfo* method) {
    g_origAi(npc, method);
    runNativeAi(npc);
}

void hookCheckActive(Il2CppObject* npc, const MethodInfo* method) {
    g_origCheckActive(npc, method);
    if (npc && !runtime::field<uint8_t>(npc, refs().active)) retireObject(npc);
}

void hookLoot(Il2CppObject* npc, const MethodInfo* method) {
    retireObject(npc);
    g_origLoot(npc, method);
}

}

bool installNpcAiHost() {
    if (g_installed) return true;
    Refs& r = refs();
    if (!r.ok || !csharpmods::hasNpcBehavior()) return false;
    if (!hook::install(r.ai, hookAi, &g_origAi)) {
        BL_WARN("csharp NPC: falha no hook NPC.AI");
        return false;
    }
    if (r.checkActive && !hook::install(r.checkActive, hookCheckActive, &g_origCheckActive))
        BL_WARN("csharp NPC: hook CheckActive indisponivel");
    if (r.loot && !hook::install(r.loot, hookLoot, &g_origLoot))
        BL_WARN("csharp NPC: hook NPCLoot indisponivel");
    g_installed = true;
    BL_INFO("csharp NPC: AI NativeAOT seletiva instalada");
    return true;
}

}
