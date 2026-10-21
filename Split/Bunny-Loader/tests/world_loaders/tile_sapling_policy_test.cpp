#include "experiments/csharp/NativeTileSaplingPolicy.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp;
    bool allow=false;
    assert(tile_sapling::validInput(4,6,-64));
    assert(tile_sapling::validInput(100000,100000,64));
    assert(!tile_sapling::validInput(-1,5,0));
    assert(!tile_sapling::validInput(4,-1,0));
    assert(!tile_sapling::validInput(4,5,65));
    assert(!tile_sapling::validInput(4,5,-65));
    assert(tile_sapling::parseReply("1",&allow)&&allow);
    assert(tile_sapling::parseReply("0",&allow)&&!allow);
    assert(!tile_sapling::parseReply("DEFAULT",&allow));
    assert(!tile_sapling::parseReply("1|1",&allow));
    assert(!tile_sapling::parseReply("true",&allow));
    assert(!tile_sapling::parseReply("1",nullptr));
    assert(tile_policy::validCallbackMask(1<<28));
    assert(!tile_policy::validCallbackMask(1<<29));
    std::cout<<"CSHARP_TILE_GROWSAPLING_BOUNDED_NATIVE_REPLY_POLICY=PASS\n";
}
