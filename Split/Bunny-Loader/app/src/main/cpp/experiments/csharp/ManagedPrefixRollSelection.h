#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace bl::csharp {
inline int weightedPrefixCandidate(const int32_t* values, size_t length, double draw) {
    if (!values || length == 0 || length > 512 ||
        !std::isfinite(draw) || draw < 0.0 || draw >= 1.0) return -1;
    const size_t index = std::min(length - 1,
        static_cast<size_t>(draw * static_cast<double>(length)));
    return values[index];
}
}
