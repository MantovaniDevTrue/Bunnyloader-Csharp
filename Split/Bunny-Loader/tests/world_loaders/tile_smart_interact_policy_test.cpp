#include "experiments/csharp/NativeTileSmartInteractPolicy.h"
#include <cassert>
#include <climits>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_smart;
    Bounds b{};
    assert(bounds(1,2,3,4,100,100,&b));
    assert(b.left==1&&b.top==2&&b.right==4&&b.bottom==5);
    assert(bounds(-3,-4,1,2,100,100,&b));
    assert(b.left==0&&b.top==0&&b.right==2&&b.bottom==3);
    assert(bounds(97,98,110,115,100,100,&b));
    assert(b.left==97&&b.top==98&&b.right==100&&b.bottom==100);
    assert(!bounds(3,0,2,0,100,100,&b));
    assert(!bounds(0,0,0,0,0,100,&b));
    assert(!bounds(0,0,64,0,100,100,&b));
    assert(!bounds(0,0,0,64,100,100,&b));
    assert(!bounds(INT_MIN,0,INT_MAX,1,100,100,&b));
    assert(!bounds(10,10,20,20,100,100,nullptr));
    assert(!bounds(1000,1000,1010,1010,100,100,&b));
    assert(eligible(1<<4,0)&&eligible(0,1)&&eligible(1<<4,1));
    assert(!eligible(0,0)&&!eligible(0,2)&&!eligible(1<<9,0));
    std::cout<<"CSHARP_TILE_SMART_INTERACT_ORIGINAL_RANGE_AND_RIGHT_CLICK=PASS\n";
}
