#include "experiments/csharp/NativeTreeSoilPolicy.h"
#include <cassert>
#include <map>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_soil;
    std::map<int,int> soils{};
    assert(customTreeType==13);
    assert(!supportedGround(753,900));
    assert(!supportedGround(900,20));
    assert(!supportedGround(65536,900));
    assert(supportedGround(754,900));
    assert(canRegister(soils,754,900));
    soils[754]=900;
    assert(owns(soils,754));
    assert(!owns(soils,755));
    assert(canRegister(soils,754,900));
    assert(!canRegister(soils,754,901));
    assert(canRegister(soils,755,901));
    assert(!canRegister(soils,2,901));
    std::cout<<"CSHARP_TREE_SOIL_NATIVE_REGISTRY_CUSTOM13=PASS\n";
}
