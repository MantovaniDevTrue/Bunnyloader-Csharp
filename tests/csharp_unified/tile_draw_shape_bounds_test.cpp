#include "content/tiles/NativeTileDrawShapeBounds.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    using bl::runtime::tile_draw_shape::valid;
    constexpr uintptr_t giant = 1989685552640ULL / sizeof(int32_t);
    static_assert(valid(1, 1));
    static_assert(valid(2, 2));
    static_assert(valid(4, 1));
    static_assert(valid(256, 256));
    static_assert(!valid(0, 1));
    static_assert(!valid(-1, 1));
    static_assert(!valid(257, 1));
    static_assert(!valid(2, 0));
    static_assert(!valid(2, 257));
    static_assert(!valid(2, giant));
    assert(valid(3, 3));
    assert(!valid(1, UINTPTR_MAX));
    std::puts("TILE_DRAW_COORDINATE_BOUNDS=PASS");
}
