#include "experiments/csharp/TerrariaHost.h"

#include "content/common/GameRefs.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <chrono>
#include <cstdint>

namespace bl::csharp {

namespace {

struct Vector2 {
    float x;
    float y;
};

struct HostRefs {
    bool tried = false;
    bool ok = false;
    FieldInfo* players = nullptr;
    FieldInfo* npcs = nullptr;
    FieldInfo* projectiles = nullptr;
    const MethodInfo* getMyPlayer = nullptr;
    int32_t playerLife = -1;
    int32_t playerLifeMax = -1;
    int32_t npcActive = -1;
    int32_t npcType = -1;
    int32_t npcLife = -1;
    int32_t npcLifeMax = -1;
    int32_t npcFriendly = -1;
    int32_t npcDontTakeDamage = -1;
};

HostRefs& refs() {
    static HostRefs r;
    if (r.tried) return r;
    r.tried = true;

    using namespace il2cpp;
    auto& a = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    if (!main || !player || !npc) return r;

    r.players = findField(main, "player");
    r.npcs = findField(main, "npc");
    r.projectiles = findField(main, "projectile");
    r.getMyPlayer = a.class_get_method_from_name(main, "get_myPlayer", 0);
    r.playerLife = fieldOffset(player, "statLife");
    r.playerLifeMax = fieldOffset(player, "statLifeMax2");
    if (r.playerLifeMax < 0) r.playerLifeMax = fieldOffset(player, "statLifeMax");

    r.npcActive = fieldOffset(npc, "active");
    r.npcType = fieldOffset(npc, "type");
    r.npcLife = fieldOffset(npc, "life");
    r.npcLifeMax = fieldOffset(npc, "lifeMax");
    r.npcFriendly = fieldOffset(npc, "friendly");
    r.npcDontTakeDamage = fieldOffset(npc, "dontTakeDamage");

    r.ok = r.players && r.npcs && r.projectiles && r.getMyPlayer &&
           r.playerLife >= 0 && r.playerLifeMax >= 0 &&
           r.npcActive >= 0 && r.npcType >= 0 && r.npcLife >= 0 && r.npcLifeMax >= 0;

    if (!r.ok) {
        BL_WARN("csharp host: refs incompletas (player=%p npc=%p proj=%p myPlayer=%p life=%d lifeMax=%d npcActive=%d)",
                (void*)r.players, (void*)r.npcs, (void*)r.projectiles,
                (void*)r.getMyPlayer, r.playerLife, r.playerLifeMax, r.npcActive);
    }
    return r;
}

Il2CppArray* staticArray(FieldInfo* field) {
    Il2CppArray* value = nullptr;
    if (field) il2cpp::api().field_static_get_value(field, &value);
    return value;
}

int unboxInt(Il2CppObject* boxed) {
    if (!boxed) return -1;
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject));
}

int localPlayerIndex(const HostRefs& r) {
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

}

TerrariaSnapshot captureTerrariaSnapshot(bool includeProjectiles, bool includeNpcs) {
    TerrariaSnapshot out;
    const auto start = std::chrono::steady_clock::now();

    if (!runtime::resolveGameRefs()) return out;
    const HostRefs& r = refs();
    if (!r.ok) return out;

    Il2CppArray* players = staticArray(r.players);
    Il2CppArray* npcs = staticArray(r.npcs);
    Il2CppArray* projectiles = includeProjectiles ? staticArray(r.projectiles) : nullptr;
    if (!players || !npcs || (includeProjectiles && !projectiles)) return out;

    const int me = localPlayerIndex(r);
    if (me < 0 || static_cast<uintptr_t>(me) >= players->length) return out;
    auto** playerData = reinterpret_cast<Il2CppObject**>(arrayData(players));
    Il2CppObject* player = playerData[me];
    if (!player) return out;

    const auto& g = runtime::game();
    const Vector2 playerCenter = centerOf(player);
    const Vector2 playerVelocity = runtime::field<Vector2>(player, g.entity.velocity);
    out.player.index = me;
    out.player.life = runtime::field<int32_t>(player, r.playerLife);
    out.player.lifeMax = runtime::field<int32_t>(player, r.playerLifeMax);
    out.player.x = playerCenter.x;
    out.player.y = playerCenter.y;
    out.player.vx = playerVelocity.x;
    out.player.vy = playerVelocity.y;

    if (includeNpcs) {
        out.npcs.resize(npcs->length);
        auto** npcData = reinterpret_cast<Il2CppObject**>(arrayData(npcs));
        for (uintptr_t i = 0; i < npcs->length; ++i) {
            NpcSample& sample = out.npcs[i];
            sample.index = static_cast<int32_t>(i);
            Il2CppObject* npc = npcData[i];
            if (!npc) continue;
    
            const bool active = runtime::field<uint8_t>(npc, r.npcActive) != 0;
            const bool friendly = r.npcFriendly >= 0 && runtime::field<uint8_t>(npc, r.npcFriendly) != 0;
            const bool dontTakeDamage = r.npcDontTakeDamage >= 0 &&
                                        runtime::field<uint8_t>(npc, r.npcDontTakeDamage) != 0;
            sample.flags = (active ? 1 : 0) | (friendly ? 2 : 0) | (dontTakeDamage ? 4 : 0);
            sample.type = runtime::field<int32_t>(npc, r.npcType);
            sample.life = runtime::field<int32_t>(npc, r.npcLife);
            sample.lifeMax = runtime::field<int32_t>(npc, r.npcLifeMax);
            const Vector2 center = centerOf(npc);
            const Vector2 velocity = runtime::field<Vector2>(npc, g.entity.velocity);
            sample.x = center.x;
            sample.y = center.y;
            sample.vx = velocity.x;
            sample.vy = velocity.y;
            if (active) ++out.activeNpcs;
        }
    }

    if (includeProjectiles) {
        out.projectiles.resize(projectiles->length);
        auto** projectileData = reinterpret_cast<Il2CppObject**>(arrayData(projectiles));
        for (uintptr_t i = 0; i < projectiles->length; ++i) {
            ProjectileSample& sample = out.projectiles[i];
            sample.index = static_cast<int32_t>(i);
            Il2CppObject* projectile = projectileData[i];
            if (!projectile) continue;
    
            const bool active = runtime::field<uint8_t>(projectile, g.proj.active) != 0;
            sample.flags = active ? 1 : 0;
            sample.type = runtime::field<int32_t>(projectile, g.proj.type);
            sample.owner = runtime::field<int32_t>(projectile, g.proj.owner);
            sample.timeLeft = runtime::field<int32_t>(projectile, g.proj.timeLeft);
            sample.damage = runtime::field<int32_t>(projectile, g.proj.damage);
            const Vector2 center = centerOf(projectile);
            const Vector2 velocity = runtime::field<Vector2>(projectile, g.entity.velocity);
            sample.x = active ? center.x : 0.0f;
            sample.y = active ? center.y : 0.0f;
            sample.vx = active ? velocity.x : 0.0f;
            sample.vy = active ? velocity.y : 0.0f;
            if (active) ++out.activeProjectiles;
        }
    }

    out.ready = true;
    out.captureMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return out;
}

}
