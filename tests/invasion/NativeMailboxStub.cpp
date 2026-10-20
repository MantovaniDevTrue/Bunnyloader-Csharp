#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.h"

#include <cstdint>

namespace bl::csharp {

InvasionSpawnResult spawnNativeInvasionNpc(const InvasionSpawnRequest& request) {
    InvasionSpawnResult result;
    result.epoch = request.epoch;
    result.ticket = request.ticket;
    result.error = InvasionSpawnError::None;
    result.npcType = 700;
    result.npcSlot = request.ticket % 200;
    result.identity = static_cast<uintptr_t>(request.ticket + 1000);
    return result;
}

}

extern "C" void bunny_invasion_test_world(int ready, int64_t session) {
    bl::csharp::setNativeInvasionWorldState(ready != 0, static_cast<uint64_t>(session));
}

extern "C" void bunny_invasion_test_pump() {
    bl::csharp::pumpNativeInvasionSpawnsOnGameThread();
}
