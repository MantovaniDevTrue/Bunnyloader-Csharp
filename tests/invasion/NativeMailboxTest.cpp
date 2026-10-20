#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.h"

#include <cassert>
#include <cstdint>
#include <limits>

extern "C" int64_t bunny_invasion_world_session();
extern "C" int bunny_invasion_submit(int64_t,int,int,const char*,const char*,int,float,float);
extern "C" int bunny_invasion_poll(int64_t,const char*,int*,int*,int*,int*,int64_t*);

namespace bl::csharp {

InvasionSpawnResult spawnNativeInvasionNpc(const InvasionSpawnRequest& r) {
    InvasionSpawnResult result;
    result.epoch = r.epoch;
    result.ticket = r.ticket;
    result.error = InvasionSpawnError::None;
    result.npcSlot = r.ticket % 200;
    result.identity = static_cast<uintptr_t>(r.ticket + 1000);
    return result;
}

}

int main() {
    using bl::csharp::pumpNativeInvasionSpawnsOnGameThread;
    using bl::csharp::setNativeInvasionWorldState;
    constexpr const char* mod = "mod-a";
    constexpr const char* npc = "ExampleSlimeNPC";
    setNativeInvasionWorldState(false, 19);
    assert(bunny_invasion_world_session() == 0);
    assert(bunny_invasion_submit(19, 1, 1, mod, npc, 0, 1600, 1800) == 0);
    setNativeInvasionWorldState(true, 19);
    assert(bunny_invasion_world_session() == 19);
    assert(bunny_invasion_submit(19, 1, 1, mod, npc, 0, 1600, 1800) == 1);
    assert(bunny_invasion_submit(19, 1, 1, mod, npc, 0, 1600, 1800) == 0);
    assert(bunny_invasion_submit(19, 1, 2, mod, npc, 0,
                                 std::numeric_limits<float>::quiet_NaN(), 1800) == 0);
    assert(bunny_invasion_submit(18, 1, 2, mod, npc, 0, 1600, 1800) == 0);
    int epoch = 0, ticket = 0, error = -1, slot = -1;
    int64_t identity = 0;
    assert(bunny_invasion_poll(19, mod, &epoch, &ticket, &error, &slot, &identity) == 0);
    pumpNativeInvasionSpawnsOnGameThread();
    assert(bunny_invasion_poll(19, "mod-b", &epoch, &ticket, &error, &slot, &identity) == 0);
    assert(bunny_invasion_poll(19, mod, &epoch, &ticket, &error, &slot, &identity) == 1);
    assert(epoch == 1 && ticket == 1 && error == 0 && slot == 1 && identity == 1001);
    assert(bunny_invasion_poll(19, mod, &epoch, &ticket, &error, &slot, &identity) == 0);

    for (int id = 1; id <= 32; ++id)
        assert(bunny_invasion_submit(19, 2, id, mod, npc, 0, 1600, 1800) == 1);
    assert(bunny_invasion_submit(19, 2, 33, mod, npc, 0, 1600, 1800) == 0);
    setNativeInvasionWorldState(false, 20);
    assert(bunny_invasion_world_session() == 0);
    pumpNativeInvasionSpawnsOnGameThread();
    setNativeInvasionWorldState(true, 20);
    assert(bunny_invasion_poll(20, mod, &epoch, &ticket, &error, &slot, &identity) == 0);
    assert(bunny_invasion_submit(19, 2, 1, mod, npc, 0, 1600, 1800) == 0);
    assert(bunny_invasion_submit(20, 3, 1, mod, npc, 0, 1600, 1800) == 1);
    pumpNativeInvasionSpawnsOnGameThread();
    assert(bunny_invasion_poll(20, mod, &epoch, &ticket, &error, &slot, &identity) == 1);
    assert(epoch == 3 && ticket == 1);

    bl::csharp::clearNativeInvasionMailbox();
    assert(bunny_invasion_world_session() == 0);
    return 0;
}
