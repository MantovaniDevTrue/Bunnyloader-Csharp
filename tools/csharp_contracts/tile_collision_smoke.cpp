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
    bool platformFalls=true;
    assert(bl::csharp::managedTilePlatformFallThrough(1,&platformFalls));
    assert(!platformFalls);
    assert(bl::csharp::managedTilePlatformFallThrough(2,&platformFalls));
    assert(platformFalls);
    assert(!bl::csharp::managedTilePlatformFallThrough(0,&platformFalls));
    assert(platformFalls);
    assert(!bl::csharp::managedTilePlatformFallThrough(3,&platformFalls));
    assert(!bl::csharp::managedTilePlatformFallThrough(1,nullptr));
    bl::csharp::ManagedTileCollisionBox box{4.f,6.f,42,30};
    assert(bl::csharp::applyManagedTileCollisionBox(&box,18,12,2.f,-3.f));
    assert(box.width==18&&box.height==12&&box.anchorX==6.f&&box.anchorY==3.f);
    assert(bl::csharp::applyManagedTileCollisionBox(&box,0,0,0.f,0.f));
    assert(box.width==18&&box.height==12&&box.anchorX==6.f);
    bl::csharp::ManagedTileCollisionBox fresh{4.f,6.f,42,30};
    assert(bl::csharp::applyManagedTileCollisionBox(&fresh,18,12,2.f,-3.f));
    assert(fresh.anchorX==6.f&&fresh.anchorY==3.f);
    bl::csharp::ManagedTileCollisionBox nextCall{4.f,6.f,42,30};
    assert(bl::csharp::applyManagedTileCollisionBox(&nextCall,18,12,2.f,-3.f));
    assert(nextCall.anchorX==fresh.anchorX&&nextCall.anchorY==fresh.anchorY);
    const auto beforeInvalid=box;
    assert(!bl::csharp::applyManagedTileCollisionBox(&box,513,5,0.f,0.f));
    assert(!bl::csharp::applyManagedTileCollisionBox(&box,0,0,257.f,0.f));
    assert(!bl::csharp::applyManagedTileCollisionBox(&box,0,0,NAN,0.f));
    assert(box.width==beforeInvalid.width&&box.anchorX==beforeInvalid.anchorX);
    assert(!bl::csharp::applyManagedTileCollisionBox(nullptr,12,12,0.f,0.f));
    int collisions=0;
    assert(bl::csharp::managedTileCollisionBudgetAllows(collisions,3));
    for(int i=0;i<3;++i)assert(bl::csharp::recordManagedTileCollisionBudget(&collisions,3));
    assert(collisions==3);
    assert(!bl::csharp::managedTileCollisionBudgetAllows(collisions,3));
    assert(!bl::csharp::recordManagedTileCollisionBudget(&collisions,3));
    collisions=0;
    assert(bl::csharp::managedTileCollisionBudgetAllows(collisions,3));
    assert(bl::csharp::recordManagedTileCollisionBudget(&collisions,1));
    assert(!bl::csharp::recordManagedTileCollisionBudget(&collisions,1));
    assert(bl::csharp::managedTileCollisionBudgetAllows(collisions,0));
    assert(bl::csharp::recordManagedTileCollisionBudget(&collisions,0));
    assert(collisions==1);
    assert(!bl::csharp::managedTileCollisionBudgetAllows(-1,1));
    assert(!bl::csharp::managedTileCollisionBudgetAllows(0,65));
    const auto invalid=resolveManagedTileCollision(99,initial,wall,true);
    assert(invalid.x==0.f && invalid.y==5.f);
    std::cout<<"MANAGED_TILE_COLLISION_POLICY_PASS\n";
}
