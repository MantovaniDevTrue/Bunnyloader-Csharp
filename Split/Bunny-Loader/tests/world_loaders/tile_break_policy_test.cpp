#include "experiments/csharp/NativeTilePolicy.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::tile_policy;
    assert(validCallbackMask(0));
    assert(validCallbackMask((1<<0)|(1<<3)|(1<<4)|(1<<12)));
    assert(validCallbackMask(1<<13));
    assert(validCallbackMask(1<<14));
    assert(validCallbackMask(1<<15));
    assert(validCallbackMask(1<<16));
    assert(validCallbackMask((1<<16)|(1<<5)));
    assert(validCallbackMask(1<<17));
    assert(validCallbackMask(1<<18));
    assert(validCallbackMask(1<<19));
    assert(validCallbackMask(1<<20));
    assert(validCallbackMask(1<<21));
    assert(validCallbackMask(1<<22));
    assert(validCallbackMask(1<<23));
    assert(!validCallbackMask(1<<24));
    assert(validCallbackMask((1<<13)|(1<<14)|(1<<15)));
    assert(validCallbackMask(1<<5));
    assert(validCallbackMask(1<<6));
    assert(validCallbackMask(1<<9));
    assert(validCallbackMask(1<<10));
    assert(validCallbackMask(1<<11));
    assert(!validCallbackMask(-1));
    assert(!blocksCanKillTile(true));
    assert(blocksCanKillTile(false));
    assert(shouldDrop(0, false, true));
    assert(!shouldDrop(0, false, false));
    assert(!shouldDrop(1, false, true));
    assert(shouldDrop(1, true, true));
    assert(!shouldDrop(1, true, false));
    assert(!shouldDrop(99, false, false));
    assert(!shouldDrop(2,false,true));
    assert(!shouldDrop(3,false,true));
    assert(shouldDrop(2,true,true));
    assert(shouldDrop(3,true,true));
    std::cout << "CSHARP_TILE_BREAK_AND_DROP_POLICIES=PASS\n";
}
