#pragma once

#include <cstdint>
#include <cmath>
#include "WorldAuthority.h"
#include <string>

namespace bl::csharp {

enum class InvasionSpawnError {
    None,
    InvalidRequest,
    NotAuthoritative,
    WorldUnavailable,
    UnregisteredNpc,
    NativeUnavailable,
    NativeException,
    NoNpcSlot,
    VerificationFailed,
    NoSafeSpawnSite
};

struct InvasionSpawnRequest {
    std::string modId;
    std::string npcClass;
    int epoch = 0;
    int ticket = 0;
    int playerIndex = -1;
    float x = 0.f;
    float y = 0.f;
    bool worldReady = false;
};

struct InvasionSpawnResult {
    InvasionSpawnError error = InvasionSpawnError::InvalidRequest;
    int epoch = 0;
    int ticket = 0;
    int npcType = -1;
    int npcSlot = -1;
    uintptr_t identity = 0;

    explicit operator bool() const noexcept {
        return error == InvasionSpawnError::None && npcSlot >= 0 && identity != 0;
    }
};

inline bool canDispatchInvasionSpawn(const InvasionSpawnRequest& request, int mode) noexcept {
    return worldIsAuthoritative(mode) && request.worldReady &&
        !request.modId.empty() && request.modId.size() <= 128 &&
        !request.npcClass.empty() && request.npcClass.size() <= 128 &&
        request.epoch > 0 && request.ticket > 0 &&
        request.playerIndex >= 0 && request.playerIndex < 255 &&
        std::isfinite(request.x) && std::isfinite(request.y) &&
        request.x >= 640.f && request.y >= 640.f &&
        request.x <= 10000000.f && request.y <= 10000000.f;
}
InvasionSpawnResult spawnNativeInvasionNpc(const InvasionSpawnRequest& request);

}
