#include "experiments/csharp/NativeTileSaplingSwapPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_sapling;
    SwapPlan plan{};
    assert(swapPlan(6,12,800,36,0,100,100,&plan));
    assert(plan.x==6&&plan.top==12&&plan.bottom==13&&plan.sourceType==800);
    assert(matchingFrames(plan,36,0,36,0,36,18));
    assert(!matchingFrames(plan,36,0,18,0,36,18));
    assert(!matchingFrames(plan,36,0,36,0,36,36));
    assert(swapPlan(6,13,800,36,18,100,100,&plan)&&plan.top==12);
    assert(!swapPlan(6,13,20,36,18,100,100,&plan));
    assert(!swapPlan(6,99,800,36,0,100,100,&plan));
    assert(!swapPlan(6,0,800,36,18,100,100,&plan));
    assert(!swapPlan(6,13,800,-1,18,100,100,&plan));
    assert(!swapPlan(6,13,800,36,-1,100,100,&plan));
    assert(!swapPlan(100,13,800,36,18,100,100,&plan));
    assert(!swapPlan(6,13,800,36,18,100,100,nullptr));
    std::cout<<"CSHARP_NATIVE_SAPLING_TWO_CELL_SWAP_PLAN=PASS\n";
}
