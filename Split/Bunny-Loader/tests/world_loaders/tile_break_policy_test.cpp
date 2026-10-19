#include "experiments/csharp/NativeTilePolicy.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::tile_policy;
    assert(!blocksCanKillTile(true));
    assert(blocksCanKillTile(false));
    assert(shouldDrop(0, false, true));
    assert(!shouldDrop(0, false, false));
    assert(!shouldDrop(1, false, true));
    assert(shouldDrop(1, true, true));
    assert(!shouldDrop(1, true, false));
    assert(!shouldDrop(99, false, false));
    std::cout << "CSHARP_TILE_BREAK_AND_DROP_POLICIES=PASS\n";
}
