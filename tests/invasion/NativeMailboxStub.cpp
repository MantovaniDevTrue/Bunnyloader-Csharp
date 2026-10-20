#include <atomic>
#include <cstring>
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionPersistence.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionRewards.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.h"

#include <cstdint>
#include <string>

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
    bl::csharp::setNativeInvasionRewardWorldState(ready != 0, static_cast<uint64_t>(session));
    const std::string path = "/tmp/bunny-invasion-ci-" + std::to_string(session) + ".wld";
    bl::csharp::setNativeInvasionSaveWorld(
        ready != 0, static_cast<uint64_t>(session), path, false);
}

extern "C" void bunny_invasion_test_world_restore(int ready, int64_t session,
                                                    const char* worldPath) {
    bl::csharp::setNativeInvasionWorldState(ready != 0, static_cast<uint64_t>(session));
    bl::csharp::setNativeInvasionRewardWorldState(ready != 0, static_cast<uint64_t>(session));
    bl::csharp::setNativeInvasionSaveWorld(
        ready != 0, static_cast<uint64_t>(session),
        worldPath ? worldPath : "", false);
}

extern "C" void bunny_invasion_test_pump() {
    bl::csharp::pumpNativeInvasionSpawnsOnGameThread();
}

namespace {
std::atomic<int> pendingUse{-1};
std::atomic<int> bindingReady{1};
}

extern "C" int bunny_item_use_bind(int64_t session, const char* mod,
                                    const char* itemName) {
    return session > 0 && mod && itemName &&
        std::strcmp(mod, "mod-a") == 0 &&
        std::strcmp(itemName, "ExampleInvasionBeacon") == 0 ? 1 : 0;
}

extern "C" int bunny_item_use_ready(int64_t session, const char* mod,
                                     const char* itemName) {
    return bindingReady.load() == 1 &&
        bunny_item_use_bind(session, mod, itemName);
}

extern "C" int bunny_item_use_poll(int64_t session, const char* mod,
                                    const char* itemName, int* playerIndex) {
    if (!session || !mod || !itemName || !playerIndex ||
        std::strcmp(mod, "mod-a") != 0 ||
        std::strcmp(itemName, "ExampleInvasionBeacon") != 0)
        return 0;
    const int pending = pendingUse.exchange(-1);
    if (pending < 0) return 0;
    *playerIndex = pending;
    return 1;
}

extern "C" void bunny_item_use_test_inject(int playerIndex) {
    pendingUse.store(playerIndex);
}

extern "C" void bunny_item_use_test_ready(int ready) {
    bindingReady.store(ready != 0 ? 1 : 0);
}

extern "C" int bunny_invasion_notice(int64_t session, const char* mod,
                                      const char* id, int phase,
                                      int progress, int goal) {
    return session > 0 && mod && id && phase >= 0 && phase <= 2 &&
        progress >= 0 && progress <= goal ? 1 : 0;
}
