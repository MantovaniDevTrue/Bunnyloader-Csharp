#include "experiments/csharp/NativeInvasionSpawn.h"
#include "experiments/csharp/NativeInvasionSpawnSite.h"

#include "content/common/GameRefs.h"
#include "content/npcs/ModNpcs.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "menu/NetRequests.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

struct Vector2 { float x, y; };

struct InvasionRefs {
    FieldInfo* mainNpc = nullptr;
    FieldInfo* mainPlayer = nullptr;
    FieldInfo* maxTilesX = nullptr;
    FieldInfo* maxTilesY = nullptr;
    FieldInfo* gameMenu = nullptr;
    const MethodInfo* getGameMenu = nullptr;
    const MethodInfo* newNpc = nullptr;
    const MethodInfo* sourceCtor = nullptr;
    Il2CppClass* sourceClass = nullptr;
    int npcActive = -1;
    int npcType = -1;
    int playerActive = -1;
    int playerLife = -1;
    bool ready = false;
};

InvasionRefs& refs() {
    static InvasionRefs r;
    if (r.ready) return r;
    if (!runtime::resolveGameRefs()) return r;
    using namespace il2cpp;
    auto& api = il2cpp::api();
    auto* main = findClass({"Terraria", "Main", {}});
    auto* npc = findClass({"Terraria", "NPC", {}});
    auto* player = findClass({"Terraria", "Player", {}});
    auto* source = findClass({"Terraria.DataStructures", "EntitySource_DebugCommand", {}});
    if (!main || !npc || !player || !source) return r;
    InvasionRefs next;
    next.mainNpc = findField(main, "npc");
    next.mainPlayer = findField(main, "player");
    next.maxTilesX = findField(main, "maxTilesX");
    next.maxTilesY = findField(main, "maxTilesY");
    next.gameMenu = findField(main, "gameMenu");
    next.getGameMenu = api.class_get_method_from_name(main, "get_gameMenu", 0);
    next.newNpc = findMethodBySignature(npc, parseSignature(
        "int NewNPC(IEntitySource source, int X, int Y, int Type, int Start, "
        "float ai0, float ai1, float ai2, float ai3, int Target)"));
    next.sourceCtor = api.class_get_method_from_name(source, ".ctor", 0);
    next.sourceClass = source;
    next.npcActive = fieldOffset(npc, "active");
    next.npcType = fieldOffset(npc, "type");
    next.playerActive = fieldOffset(player, "active");
    next.playerLife = fieldOffset(player, "statLife");
    next.ready = next.mainNpc && next.mainPlayer && next.maxTilesX &&
        next.maxTilesY && (next.gameMenu || next.getGameMenu) &&
        next.newNpc && next.sourceCtor && next.npcActive >= 0 &&
        next.npcType >= 0 && next.playerActive >= 0 && next.playerLife >= 0;
    if (next.ready) r = next;
    return r;
}

Il2CppArray* mainArray(FieldInfo* field) {
    Il2CppArray* result = nullptr;
    il2cpp::api().field_static_get_value(field, &result);
    return result;
}

Il2CppObject* arrayElement(Il2CppArray* array, int slot) {
    if (!array || slot < 0 || static_cast<uintptr_t>(slot) >= array->length) return nullptr;
    return reinterpret_cast<Il2CppObject**>(arrayData(array))[slot];
}

bool inWorld(const InvasionRefs& r) {
    if (r.getGameMenu) {
        Il2CppObject* error = nullptr;
        Il2CppObject* value = il2cpp::api().runtime_invoke(r.getGameMenu, nullptr, nullptr, &error);
        if (!error && value) {
            const bool menu = *reinterpret_cast<uint8_t*>(
                reinterpret_cast<char*>(value) + sizeof(Il2CppObject)) != 0;
            return !menu;
        }
    }
    if (!r.gameMenu) return false;
    bool menu = true;
    il2cpp::api().field_static_get_value(r.gameMenu, &menu);
    return !menu;
}

}

InvasionSpawnResult spawnNativeInvasionNpc(const InvasionSpawnRequest& request) {
    InvasionSpawnResult result;
    result.epoch = request.epoch;
    result.ticket = request.ticket;
    const int mode = runtime::netMode();
    if (!worldIsAuthoritative(mode)) {
        result.error = InvasionSpawnError::NotAuthoritative;
        return result;
    }
    if (!canDispatchInvasionSpawn(request, mode)) return result;
    if (!runtime::modNpcsSettled() || !runtime::resolveGameRefs()) {
        result.error = InvasionSpawnError::WorldUnavailable;
        return result;
    }
    const InvasionRefs& r = refs();
    if (!r.ready) {
        result.error = InvasionSpawnError::NativeUnavailable;
        return result;
    }
    if (!inWorld(r)) {
        result.error = InvasionSpawnError::WorldUnavailable;
        return result;
    }
    const int type = runtime::modNpcTypeByName(request.modId, request.npcClass);
    if (type < runtime::kVanillaNpcCount || !runtime::modNpcBelongsTo(type, request.modId)) {
        result.error = InvasionSpawnError::UnregisteredNpc;
        return result;
    }

    auto& api = il2cpp::api();
    int maxX = 0, maxY = 0;
    api.field_static_get_value(r.maxTilesX, &maxX);
    api.field_static_get_value(r.maxTilesY, &maxY);
    const float worldRight = maxX * 16.f - 640.f;
    const float worldBottom = maxY * 16.f - 640.f;
    if (maxX <= 0 || maxY <= 0 || request.x >= worldRight ||
        request.y >= worldBottom) {
        result.error = InvasionSpawnError::WorldUnavailable;
        return result;
    }

    Il2CppArray* players = mainArray(r.mainPlayer);
    Il2CppObject* player = arrayElement(players, request.playerIndex);
    if (!player || !runtime::field<uint8_t>(player, r.playerActive) ||
        runtime::field<int>(player, r.playerLife) <= 0) {
        result.error = InvasionSpawnError::WorldUnavailable;
        return result;
    }
    const auto& entity = runtime::game().entity;
    const Vector2 playerPos = runtime::field<Vector2>(player, entity.position);
    if (!std::isfinite(playerPos.x) || !std::isfinite(playerPos.y) ||
        std::fabs(request.x - playerPos.x) > 1800.f ||
        std::fabs(request.y - playerPos.y) > 1000.f) {
        result.error = InvasionSpawnError::InvalidRequest;
        return result;
    }

    InvasionSpawnSite site;
    if (!findNativeInvasionSpawnSite(static_cast<int>(request.x),
                                     static_cast<int>(request.y), &site) ||
        std::fabs(static_cast<float>(site.x) - playerPos.x) > 1400.f ||
        std::fabs(static_cast<float>(site.y) - playerPos.y) > 1000.f) {
        result.error = InvasionSpawnError::NoSafeSpawnSite;
        return result;
    }

    Il2CppArray* npcs = mainArray(r.mainNpc);
    if (!npcs) {
        result.error = InvasionSpawnError::WorldUnavailable;
        return result;
    }
    bool freeSlot = false;
    for (uintptr_t i = 0; i < std::min<uintptr_t>(npcs->length, 200); ++i) {
        Il2CppObject* npc = arrayElement(npcs, static_cast<int>(i));
        if (!npc || !runtime::field<uint8_t>(npc, r.npcActive)) {
            freeSlot = true;
            break;
        }
    }
    if (!freeSlot) {
        result.error = InvasionSpawnError::NoNpcSlot;
        return result;
    }

    Il2CppObject* source = api.object_new(r.sourceClass);
    if (!source) {
        result.error = InvasionSpawnError::NativeUnavailable;
        return result;
    }
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(r.sourceCtor, source, nullptr, &exception);
    if (exception) {
        result.error = InvasionSpawnError::NativeException;
        return result;
    }

    int x = site.x, y = site.y;
    int npcType = type, start = 0, target = request.playerIndex;
    float ai0 = 0.f, ai1 = 0.f, ai2 = 0.f, ai3 = 0.f;
    void* args[] = {source, &x, &y, &npcType, &start, &ai0, &ai1, &ai2, &ai3, &target};
    exception = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(r.newNpc, nullptr, args, &exception);
    if (exception || !boxed) {
        result.error = InvasionSpawnError::NativeException;
        return result;
    }

    const int slot = *reinterpret_cast<int*>(
        reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject));
    Il2CppObject* npc = arrayElement(npcs, slot);
    if (slot < 0 || slot >= 200 || !npc ||
        !runtime::field<uint8_t>(npc, r.npcActive) ||
        runtime::field<int>(npc, r.npcType) != type) {
        result.error = InvasionSpawnError::VerificationFailed;
        return result;
    }
    result.error = InvasionSpawnError::None;
    result.npcType = type;
    result.npcSlot = slot;
    result.identity = reinterpret_cast<uintptr_t>(npc);
    if ((mode & 2) != 0) runtime::sendData(23, slot);
    return result;
}

}
