#include "experiments/csharp/NativeTilePolicy.h"

#include <cassert>
#include <iostream>

using bl::csharp::tile_policy::bannerStyleFromFrameX;

int main() {
    assert(bannerStyleFromFrameX(0) == 0);
    assert(bannerStyleFromFrameX(18) == 1);
    assert(bannerStyleFromFrameX(36) == 2);
    assert(bannerStyleFromFrameX(18 * 49) == 49);
    assert(bannerStyleFromFrameX(18 * 1024) == 1024);
    assert(bannerStyleFromFrameX(-18) == -1);
    assert(bannerStyleFromFrameX(17) == -1);
    assert(bannerStyleFromFrameX(19) == -1);
    assert(bannerStyleFromFrameX(18 * 1025) == -1);
    assert(bl::csharp::tile_policy::shouldDrop(2, false, true) == false);
    assert(bl::csharp::tile_policy::shouldDrop(2, true, true) == true);
    assert(bl::csharp::tile_policy::shouldDrop(2, true, false) == false);
    std::cout << "Banner tile style PASS (exact style mapping, large object, bounds)\n";
}
