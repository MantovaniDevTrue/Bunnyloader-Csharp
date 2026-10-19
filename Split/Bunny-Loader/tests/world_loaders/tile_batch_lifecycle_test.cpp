#include "experiments/csharp/NativeTileBatchPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_sprite;
    assert(chooseBatchAction(false,true,false)==BatchAction::Reject);
    assert(chooseBatchAction(true,false,false)==BatchAction::Reject);
    assert(chooseBatchAction(true,true,true)==BatchAction::Reuse);
    assert(chooseBatchAction(true,true,false)==BatchAction::Begin);
    assert(!shouldEndBatch(BatchAction::Reuse,true));
    assert(!shouldEndBatch(BatchAction::Begin,false));
    assert(shouldEndBatch(BatchAction::Begin,true));
    std::cout<<"CSHARP_TILE_SPRITE_BATCH_LIFECYCLE=PASS\n";
}
