#include "experiments/csharp/NativeTileMiningPolicy.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
int main() {
    using namespace bl::csharp::tile_mining;
    assert(valid(0,1.f)&&valid(1000,0.f)&&valid(10,2.5f));
    assert(!valid(-1,1.f)&&!valid(1001,1.f)&&!valid(5,-1.f));
    assert(!valid(5,std::numeric_limits<float>::infinity()));
    assert(!valid(5,std::numeric_limits<float>::quiet_NaN()));
    assert(!valid(5,129.f));
    assert(apply(24,40,41,1.f)==0);
    assert(apply(24,41,41,1.f)==24);
    assert(apply(24,41,41,2.5f)==9);
    assert(apply(24,41,41,0.f)==24);
    assert(apply(0,41,41,2.f)==0);
    assert(apply(-1,41,41,2.f)==-1);
    assert(apply(INT32_MAX,50,0,0.01f)==INT32_MAX);
    std::cout<<"CSHARP_TILE_MINING_MINPICK_MINERESIST=PASS\n";
}
