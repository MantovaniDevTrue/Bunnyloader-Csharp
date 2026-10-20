#include "experiments/csharp/NativeTileWirePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_wire;
    for(std::size_t n=0;n<=25;++n) {
        const bool allow=n==13||n==14||n==16||n==17||n==19||n==20||n==21||n==22||n==23||n==25;
        assert(supported(n)==allow);
        assert(hasAnimationHeight(n)==(allow&&n>=14));
        assert(hasAnimationPolicy(n)==(n==16||n==17||n==19||n==20||n==21||n==22||n==23||n==25));
        assert(hasAdjacencies(n)==(n==17||n==19||n==20||n==21||n==22||n==23||n==25));
        assert(hasBreakPolicy(n)==(n==19||n==20||n==21||n==22||n==23||n==25));
        assert(hasCallbacks(n)==(n==20||n==21||n==22||n==23||n==25));
        assert(hasDrawCachePolicy(n)==(n==21||n==22||n==23||n==25));
        assert(hasFrameToggle(n)==(n==22||n==23||n==25));
        assert(hasRegisteredDrops(n)==(n==23||n==25));
        assert(hasMiningPolicy(n)==(n==25));
    }
    std::cout<<"CSHARP_TILE_WIRE_25_MINING_BACKWARD_COMPATIBILITY=PASS\n";
}
