#pragma once

#include <cstdint>
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
    VerificationFailed
};

struct InvasionSpawnRequest {
    std::string modId;
    std::string npcClass;
    int playerIndex = -1;
    float x = 0.f;
    float y = 0.f;
    bool worldReady = false;
};

struct InvasionSpawnResult {
    InvasionSpawnError error = InvasionSpawnError::InvalidRequest;
    int npcType = -1;
    int npcSlot = -1;
    uintptr_t identity = 0;

    explicit operator bool() const noexcept {
        return error == InvasionSpawnError::None && npcSlot >= 0 && identity != 0;
    }
};

bool canDispatchInvasionSpawn(const InvasionSpawnRequest& request, int mode) noexcept;
InvasionSpawnResult spawnNativeInvasionNpc(const InvasionSpawnRequest& request);

}
