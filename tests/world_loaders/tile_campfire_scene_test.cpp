#include "experiments/csharp/NativeTileCampfirePolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    using namespace bl::csharp::tile_campfire;
    const auto center=area(5*16.f+8,5*16.f+8,10,10,50,50);
    assert(center.left==0&&center.top==0&&center.right==10&&center.bottom==10);
    const auto corner=area(8,8,10,10,50,50);
    assert(corner.left==0&&corner.top==0&&corner.right==5&&corner.bottom==5);
    const auto edge=area(49*16.f,49*16.f,10,10,50,50);
    assert(edge.left==44&&edge.top==44&&edge.right==50&&edge.bottom==50);
    const auto odd=area(10*16.f,10*16.f,9,9,50,50);
    assert(odd.left==6&&odd.top==6&&odd.right==14&&odd.bottom==14);
    const auto none=area(4,4,0,10,50,50);
    assert(none.right==0&&none.bottom==0);
    const auto invalid=area(std::numeric_limits<float>::quiet_NaN(),0,10,10,50,50);
    assert(invalid.right==0&&invalid.bottom==0);
    assert(area(0,0,257,10,50,50).right==0);
    assert(area(0,0,10,300,50,50).right==0);
    float px=-1.f,py=-1.f;
    assert(soundPosition(12,9,&px,&py)&&px==192.f&&py==144.f);
    assert(!soundPosition(-1,0,&px,&py)&&!soundPosition(0,-1,&px,&py));
    assert(!soundPosition(100001,0,&px,&py)&&!soundPosition(0,100001,&px,&py));
    assert(!soundPosition(0,0,nullptr,&py)&&!soundPosition(0,0,&px,nullptr));
    assert(lit(true,0)&&lit(true,18)&&lit(true,35));
    assert(!lit(true,36)&&!lit(true,54)&&!lit(false,0)&&!lit(true,-1));
    std::cout<<"CSHARP_CAMPFIRE_SCENE_BOUNDS_AND_STATE=PASS\n";
}
