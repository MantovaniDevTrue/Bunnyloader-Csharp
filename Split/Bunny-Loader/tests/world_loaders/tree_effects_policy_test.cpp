#include "experiments/csharp/NativeTreeEffectsPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_effect;
    assert(valid(-1,7)&&valid(0,0)&&valid(500,1999));
    assert(!valid(-2,7)&&!valid(32768,7)&&!valid(-1,-1)&&!valid(-1,2000));
    assert(acceptLeaf(754,true,-1));
    assert(acceptLeaf(754,true,500));
    assert(!acceptLeaf(753,true,500));
    assert(!acceptLeaf(754,false,500));
    assert(!acceptLeaf(65536,true,500));
    assert(acceptDust(5,7));
    assert(acceptDust(5,1999));
    assert(!acceptDust(6,7));
    assert(!acceptDust(5,2000));
    std::cout<<"CSHARP_TREE_EFFECT_POLICY=PASS\n";
}
