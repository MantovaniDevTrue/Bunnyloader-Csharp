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

    using bl::runtime::tile_draw_shape::reconstructHeights;
    int h[4] = {};
    // ExampleSapling: CoordinateHeights {16,18}, padding 2 -> FullHeight 38.
    assert(reconstructHeights(2, 38, 2, h, 4) && h[0] == 16 && h[1] == 18);
    // Workbench (Style2x1 com uma altura): {16} e {18}.
    assert(reconstructHeights(1, 18, 2, h, 4) && h[0] == 16);
    assert(reconstructHeights(1, 20, 2, h, 4) && h[0] == 18);
    // Estilo vanilla de 3 linhas de 16.
    assert(reconstructHeights(3, 54, 2, h, 4) && h[0] == 16 && h[1] == 16 && h[2] == 16);
    // Sem padding.
    assert(reconstructHeights(2, 34, 0, h, 4) && h[0] == 16 && h[1] == 18);
    // Rejeicoes: soma impossivel, capacidade, padding absurdo, linhas invalidas.
    assert(!reconstructHeights(2, 18, 2, h, 4));
    assert(!reconstructHeights(3, 54, 2, h, 2));
    assert(!reconstructHeights(1, 18, 65, h, 4));
    assert(!reconstructHeights(0, 18, 2, h, 4));
    assert(!reconstructHeights(257, 18, 2, h, 4));
    assert(!reconstructHeights(1, 0, 2, h, 4));
    assert(!reconstructHeights(1, 1000, 2, h, 4));
    std::puts("TILE_DRAW_COORDINATE_BOUNDS=PASS");
}
