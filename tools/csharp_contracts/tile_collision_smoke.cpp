#include "experiments/csharp/ManagedTileCollisionPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>

using bl::csharp::TileCollisionVector;
using bl::csharp::managedTileCollisionModeSupported;
using bl::csharp::resolveManagedTileCollision;
using bl::csharp::shouldDeferManagedTileCollisionKill;

int main() {
    const TileCollisionVector initial{8.f, 5.f};
    const TileCollisionVector wall{0.f, 5.f};
    const TileCollisionVector floor{8.f, 0.f};
    const TileCollisionVector corner{0.f, 0.f};
    assert(!managedTileCollisionModeSupported(0));
    assert(managedTileCollisionModeSupported(1));
    assert(managedTileCollisionModeSupported(2));
    assert(managedTileCollisionModeSupported(3));
    assert(!managedTileCollisionModeSupported(4));
    for(int mode=1;mode<=3;++mode) {
        assert(shouldDeferManagedTileCollisionKill(mode,initial,wall,true));
        assert(!shouldDeferManagedTileCollisionKill(mode,initial,initial,true));
        assert(!shouldDeferManagedTileCollisionKill(mode,initial,wall,false));
        assert(!shouldDeferManagedTileCollisionKill(mode,initial,{NAN,5.f},true));
    }
    assert(!shouldDeferManagedTileCollisionKill(0,initial,wall,true));
    assert(!shouldDeferManagedTileCollisionKill(4,initial,wall,true));
    const auto keep=resolveManagedTileCollision(1,initial,wall,true);
    assert(keep.x==0.f && keep.y==5.f);
    const auto bounceWall=resolveManagedTileCollision(2,initial,wall,true);
    assert(bounceWall.x==-8.f && bounceWall.y==5.f);
    const auto bounceFloor=resolveManagedTileCollision(2,initial,floor,true);
    assert(bounceFloor.x==8.f && bounceFloor.y==-5.f);
    const auto bounceCorner=resolveManagedTileCollision(2,initial,corner,true);
    assert(bounceCorner.x==-8.f && bounceCorner.y==-5.f);
    const auto stopWall=resolveManagedTileCollision(3,initial,wall,true);
    assert(stopWall.x==0.f && stopWall.y==5.f);
    const auto stopFloor=resolveManagedTileCollision(3,initial,floor,true);
    assert(stopFloor.x==8.f && stopFloor.y==0.f);
    const auto unchanged=resolveManagedTileCollision(3,initial,initial,true);
    assert(unchanged.x==8.f && unchanged.y==5.f);
    const auto disabled=resolveManagedTileCollision(2,initial,wall,false);
    assert(disabled.x==0.f && disabled.y==5.f);
    assert(shouldDeferManagedTileCollisionKill(2,initial,wall,true,8.f));
    assert(!shouldDeferManagedTileCollisionKill(2,initial,wall,true,10.f));
    assert(shouldDeferManagedTileCollisionKill(1,initial,wall,true,10.f));
    const auto half=resolveManagedTileCollision(2,initial,wall,true,0.5f);
    assert(half.x==-4.f && half.y==5.f);
    const auto boosted=resolveManagedTileCollision(2,initial,floor,true,1.5f);
    assert(boosted.x==8.f && boosted.y==-7.5f);
    const auto threshold=resolveManagedTileCollision(2,initial,wall,true,0.5f,10.f);
    assert(threshold.x==0.f && threshold.y==5.f);
    const auto retained=resolveManagedTileCollision(2,initial,wall,true,0.5f,8.f);
    assert(retained.x==-4.f && retained.y==5.f);
    const auto noRest=resolveManagedTileCollision(2,initial,wall,true,0.f);
    assert(noRest.x==0.f && noRest.y==5.f);
    const auto rejectedRest=resolveManagedTileCollision(2,initial,wall,true,3.f);
    assert(rejectedRest.x==0.f && rejectedRest.y==5.f);
    const auto invalid=resolveManagedTileCollision(99,initial,wall,true);
    assert(invalid.x==0.f && invalid.y==5.f);
    std::cout<<"MANAGED_TILE_COLLISION_POLICY_PASS\n";
}
