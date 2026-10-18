#include "experiments/csharp/WorldEntryReadiness.h"
#include <cassert>
#include <iostream>
int main() {
    using bl::csharp::worldDependentAssetsMayDefer;
    // This path intentionally has no elapsed-time argument: resolved core need not wait 6 s.
    assert(worldDependentAssetsMayDefer(0,0,0,true,true,true));
    assert(!worldDependentAssetsMayDefer(1,0,0,true,true,true));
    assert(!worldDependentAssetsMayDefer(0,1,0,true,true,true));
    assert(!worldDependentAssetsMayDefer(0,0,1,true,true,true));
    assert(!worldDependentAssetsMayDefer(0,0,0,false,true,true));
    assert(!worldDependentAssetsMayDefer(0,0,0,true,false,true));
    assert(!worldDependentAssetsMayDefer(0,0,0,true,true,false));
    assert(!worldDependentAssetsMayDefer(-1,0,0,true,true,true));
    assert(!worldDependentAssetsMayDefer(0,-1,0,true,true,true));
    std::cout<<"WORLD_ENTRY_CORE_CONTRACT_NO_FIXED_ASSET_DELAY_PASS\n";
}
