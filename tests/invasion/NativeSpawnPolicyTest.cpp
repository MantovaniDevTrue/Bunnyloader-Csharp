#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionSpawn.h"

#include <cassert>
#include <limits>

int main() {
    using bl::csharp::InvasionSpawnRequest;
    using bl::csharp::canDispatchInvasionSpawn;
    InvasionSpawnRequest request;
    request.modId = "examplemodcsharp";
    request.npcClass = "ExampleSlimeNPC";
    request.worldReady = true;
    request.epoch = 1;
    request.ticket = 1;
    request.playerIndex = 1;
    request.x = 1600.f;
    request.y = 1800.f;
    assert(canDispatchInvasionSpawn(request, 0));
    assert(canDispatchInvasionSpawn(request, 2));
    assert(canDispatchInvasionSpawn(request, 3));
    assert(!canDispatchInvasionSpawn(request, 1));
    assert(!canDispatchInvasionSpawn(request, -1));
    request.worldReady = false;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.worldReady = true;
    request.modId.clear();
    assert(!canDispatchInvasionSpawn(request, 0));
    request.modId = "examplemodcsharp";
    request.npcClass.clear();
    assert(!canDispatchInvasionSpawn(request, 0));
    request.npcClass = "ExampleSlimeNPC";
    request.playerIndex = -1;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.playerIndex = 255;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.playerIndex = 0;
    request.epoch = 0;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.epoch = 1;
    request.ticket = 0;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.ticket = 1;
    request.x = 0.f;
    assert(!canDispatchInvasionSpawn(request, 0));
    request.x = std::numeric_limits<float>::quiet_NaN();
    assert(!canDispatchInvasionSpawn(request, 0));
    request.x = 1600.f;
    request.y = std::numeric_limits<float>::infinity();
    assert(!canDispatchInvasionSpawn(request, 0));
    return 0;
}
