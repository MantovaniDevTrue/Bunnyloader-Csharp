#pragma once
#include <cstddef>

namespace bl::csharp::tile_wire {
inline constexpr bool supported(std::size_t count) {
    return count == 13 || count == 14 || count == 16 || count == 17 || count == 19 || count == 20 || count == 21;
}
inline constexpr bool hasAnimationHeight(std::size_t count) {
    return supported(count) && count >= 14;
}
inline constexpr bool hasAnimationPolicy(std::size_t count) {
    return count == 16 || count == 17 || count == 19 || count == 20 || count == 21;
}
inline constexpr bool hasAdjacencies(std::size_t count) {
    return count == 17 || count == 19 || count == 20 || count == 21;
}
inline constexpr bool hasBreakPolicy(std::size_t count) {
    return count == 19 || count == 20 || count == 21;
}
inline constexpr bool hasCallbacks(std::size_t count) {
    return count == 20 || count == 21;
}
inline constexpr bool hasDrawCachePolicy(std::size_t count) {
    return count == 21;
}
}
