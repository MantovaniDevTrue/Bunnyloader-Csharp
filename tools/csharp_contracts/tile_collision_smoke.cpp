#include "experiments/csharp/ManagedTileCollisionPolicy.h"
#include <cassert>
#include <iostream>

using bl::csharp::TileCollisionVector;
using bl::csharp::shouldDeferManagedTileCollisionKill;
int main() {
    TileCollisionVector initial{8.f, 5.f};
    TileCollisionVector stopped{0.f, 5.f};
    assert(shouldDeferManagedTileCollisionKill(1,initial,stopped,true));
    assert(!shouldDeferManagedTileCollisionKill(0,initial,stopped,true));
    assert(!shouldDeferManagedTileCollisionKill(1,initial,initial,true));
    assert(!shouldDeferManagedTileCollisionKill(1,initial,stopped,false));
    assert(!shouldDeferManagedTileCollisionKill(1,initial,{NAN,5.f},true));
    std::cout<<"MANAGED_TILE_COLLISION_POLICY_PASS\n";
}
