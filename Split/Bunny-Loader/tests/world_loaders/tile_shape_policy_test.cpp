#include "experiments/csharp/NativeTilePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_policy;
    assert(validShape(0)&&validShape(1)&&validShape(2)&&validShape(3)&&validShape(4));
    assert(!validShape(-1)&&!validShape(5));
    const auto bed=footprint(1),camp=footprint(2),lamp=footprint(3),sapling=footprint(4);
    assert(bed.width==2&&bed.height==1);
    assert(sapling.width==1&&sapling.height==2&&sapling.frameWidth==18&&sapling.frameHeight==36);
    assert(camp.width==3&&camp.height==2&&camp.frameWidth==54&&camp.frameHeight==36);
    assert(lamp.width==1&&lamp.height==3&&lamp.frameWidth==18&&lamp.frameHeight==54);
    int left=-1,top=-1;
    assert(origin(2,12,9,36,18,&left,&top)&&left==10&&top==8);
    assert(origin(2,12,9,36,54,&left,&top)&&left==10&&top==8);
    assert(origin(3,20,14,18,36,&left,&top)&&left==20&&top==12);
    assert(!origin(3,20,0,0,36,&left,&top));
    assert(!origin(3,20,12,-1,36,&left,&top));
    assert(!origin(0,20,12,0,0,&left,&top));
    for(int dx=0;dx<3;++dx)
        for(int dy=0;dy<2;++dy) {
            assert(frameMatches(2,dx*18,dy*18,0,0,dx,dy));
            assert(frameMatches(2,dx*18,36+dy*18,0,36,dx,dy));
            assert(!frameMatches(2,dx*18,36+dy*18,0,0,dx,dy));
        }
    for(int dy=0;dy<3;++dy) {
        assert(frameMatches(3,0,dy*18,0,0,0,dy));
        assert(frameMatches(3,18,dy*18,18,0,0,dy));
        assert(!frameMatches(3,18,dy*18,0,0,0,dy));
    }
    assert(frameMatches(4,18,18,18,0,0,1));
    assert(!frameMatches(4,18,18,0,0,0,1));
    assert(origin(4,10,21,18,18,&left,&top)&&left==10&&top==20);
    assert(!frameMatches(4,18,0,18,0,0,2));
    assert(!frameMatches(2,0,0,0,0,3,0));
    assert(!frameMatches(3,0,0,0,0,0,3));
    std::cout<<"CSHARP_TILE_3X2_1X3_SAPLING1X2_FRAME_POLICY=PASS\n";
}
