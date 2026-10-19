#include "experiments/csharp/NativeTileWirePolicy.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::tile_wire;
    for (std::size_t n = 0; n < 20; ++n) {
        const bool allow = n == 13 || n == 14 || n == 16 || n == 17 || n == 19;
        assert(supported(n) == allow);
        assert(hasAnimationHeight(n) == (allow && n >= 14));
        assert(hasAnimationPolicy(n) == (n == 16 || n == 17 || n == 19));
        assert(hasAdjacencies(n) == (n == 17 || n == 19));
        assert(hasBreakPolicy(n) == (n == 19));
    }
    std::cout << "CSHARP_TILE_WIRE_ANIMATION_16_AND_17_COMPATIBILITY=PASS\n";
}
