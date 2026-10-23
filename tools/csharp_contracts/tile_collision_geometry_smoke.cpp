#include "experiments/csharp/ManagedTileCollisionPolicy.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
struct Rect { float x,y; int width,height; };

// Terraria 301720 HandleMovement computes the collision origin from Center
// minus the componentwise product of colSize and resizeAnchor. The anchor is
// a fraction, not a position in pixels. Evaluate this consumer independently
// of the producer so a unit/sign error cannot pass as a matching anchor value.
Rect nativeRectangle(const bl::csharp::ManagedTileCollisionBox& box) {
    return {1000.f-box.width*box.anchorX,2000.f-box.height*box.anchorY,
            box.width,box.height};
}
void near(float a,float b) {assert(std::fabs(a-b)<0.001f);}
}

int main() {
    using bl::csharp::ManagedTileCollisionBox;
    using bl::csharp::applyManagedTileCollisionBox;
    int cases=0;
    for(float anchor:{0.f,0.5f,0.75f,1.f})
    for(int width:{0,1,18,512})
    for(int height:{0,1,12,512})
    for(float dx:{-256.f,-3.f,0.f,2.5f,256.f})
    for(float dy:{-256.f,-3.f,0.f,2.5f,256.f}) {
        const ManagedTileCollisionBox vanilla{anchor,1.f-anchor,42,30};
        const int effectiveWidth=width?width:vanilla.width;
        const int effectiveHeight=height?height:vanilla.height;
        const Rect resized=nativeRectangle({vanilla.anchorX,vanilla.anchorY,
                                           effectiveWidth,effectiveHeight});
        auto actual=vanilla;
        assert(applyManagedTileCollisionBox(&actual,width,height,dx,dy));
        const Rect translated=nativeRectangle(actual);
        // Width/height keep the native anchor; translation is in world pixels
        // (+X right, +Y down), independent of the chosen collision dimensions.
        near(translated.x,resized.x+dx);near(translated.y,resized.y+dy);
        assert(translated.width==effectiveWidth&&translated.height==effectiveHeight);
        // GetCollisionParams can run twice in one movement. Each invocation
        // starts from the original native outputs and must give the same box.
        auto secondCall=vanilla;
        assert(applyManagedTileCollisionBox(&secondCall,width,height,dx,dy));
        near(nativeRectangle(secondCall).x,translated.x);
        near(nativeRectangle(secondCall).y,translated.y);
        ++cases;
    }
    // Regression for the public C# example: this used to move X by -45px.
    ManagedTileCollisionBox example{0.5f,0.5f,42,30};
    assert(applyManagedTileCollisionBox(&example,18,12,2.5f,-3.f));
    near(nativeRectangle(example).x,993.5f);
    near(nativeRectangle(example).y,1991.f);
    const auto preserved=example;
    assert(!applyManagedTileCollisionBox(&example,18,12,INFINITY,0.f));
    assert(!applyManagedTileCollisionBox(&example,18,12,0.f,NAN));
    near(example.anchorX,preserved.anchorX);near(example.anchorY,preserved.anchorY);
    assert(example.width==preserved.width&&example.height==preserved.height);
    std::cout<<"MANAGED_TILE_COLLISION_NATIVE_GEOMETRY_PASS cases="<<cases<<'\n';
}
