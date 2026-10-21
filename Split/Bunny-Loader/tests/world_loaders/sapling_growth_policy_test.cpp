#include "experiments/csharp/NativeSaplingGrowthPolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_growth;
    assert(valid(4,0)&&valid(4,1)&&valid(4,20)&&valid(4,1024));
    assert(!valid(4,-1)&&!valid(4,1025)&&!valid(0,20));
    assert(hit(20,0.0)&&hit(20,0.049999));
    assert(!hit(20,0.05)&&!hit(20,1.0)&&!hit(20,-0.1));
    assert(hit(1,0.999)&&!hit(1,1.0));
    assert(!hit(0,0.0)&&!hit(1025,0.0));
    assert(!hit(20,std::numeric_limits<double>::quiet_NaN()));
    std::cout<<"CSHARP_NATIVE_SAPLING_GROWTH_CHANCE=PASS\n";
}
