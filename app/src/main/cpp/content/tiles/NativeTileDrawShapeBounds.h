#pragma once
#include <cstdint>

namespace bl::runtime::tile_draw_shape {
inline constexpr uintptr_t maxCoordinateRows = 256;

constexpr bool valid(int rows, uintptr_t heightsLength) {
    return rows > 0 && static_cast<uintptr_t>(rows) <= maxCoordinateRows &&
           heightsLength > 0 && heightsLength <= maxCoordinateRows;
}
}
