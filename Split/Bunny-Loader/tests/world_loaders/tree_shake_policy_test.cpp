#include "experiments/csharp/NativeTreeShakePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_shake;
    int32_t xs[]={3,8,11}, ys[]={45,32,9};
    const auto canopy=canopyPosition(8,32);
    assert(canopy.x==8&&canopy.y==32&&canopy.pixelX==128&&canopy.pixelY==512);
    const auto neighbor=canopyPosition(9,35);
    assert(neighbor.x==9&&neighbor.y==35&&neighbor.pixelX==144&&neighbor.pixelY==560);
    assert(validItem(6196)&&validItem(65535));
    assert(!validItem(6195)&&!validItem(65536));
    assert(canShake(3,30,100,100,0,5,0));
    assert(canShake(3,30,100,100,4,5,2));
    assert(!canShake(3,30,100,100,5,5,0));
    assert(!canShake(3,30,100,100,0,5,1));
    assert(!canShake(0,30,100,100,0,5,0));
    assert(!canShake(3,30,100,100,0,5000,0));
    assert(repeated(xs,ys,3,2,3,45));
    assert(!repeated(xs,ys,3,2,11,9));
    assert(repeated(xs,ys,3,4,8,32));
    assert(repeated(nullptr,ys,3,2,8,32));
    assert(sendLeaf(900,true,0)&&sendLeaf(900,true,2));
    assert(!sendLeaf(-1,true,0)&&!sendLeaf(900,false,0)&&!sendLeaf(900,true,1));
    std::cout<<"CSHARP_TREE_SHAKE_POLICY=PASS\n";
}
