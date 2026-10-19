#include "experiments/csharp/NativeTileAnimationPolicy.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    using namespace bl::csharp::tile_animation;
    assert(valid(0, {1, 0}));
    assert(valid(36, {4, 5}));
    assert(!valid(0, {4, 5}));
    assert(!valid(36, {0, 0}));
    assert(!valid(36, {65, 1}));
    assert(!valid(36, {4, 0}));
    assert(!valid(36, {1, 5}));
    assert(!valid(36, {4, 601}));
    int32_t frame = 0, counter = 0;
    for (int i = 0; i < 4; ++i) advance(frame, counter, {4, 5});
    assert(frame == 0 && counter == 4);
    advance(frame, counter, {4, 5});
    assert(frame == 1 && counter == 0);
    for (int i = 0; i < 15; ++i) advance(frame, counter, {4, 5});
    assert(frame == 0 && counter == 0);
    frame = -100; counter = 900;
    advance(frame, counter, {4, 5});
    assert(frame == 0 && counter == 1);
    advance(frame, counter, {1, 0});
    assert(frame == 0 && counter == 1);
    std::cout << "CSHARP_TILE_ANIMATION_NATIVE_POLICY=PASS\n";
}
