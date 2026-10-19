#include "experiments/csharp/NativeTileDrawCachePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using bl::csharp::tile_draw_cache::enabled;
    assert(enabled(false,false,false));
    assert(!enabled(true,false,false));
    assert(!enabled(false,true,false));
    assert(!enabled(false,false,true));
    assert(!enabled(false,true,true));
    assert(!enabled(true,true,true));
    std::cout<<"CSHARP_TILE_DRAW_CACHE_PARITY=PASS\n";
}
