#pragma once

#include <cstdint>

namespace bl::csharp {

inline bool managedDustDecisionCurrent(int phase, std::uint64_t now,
                                       std::uint64_t requestFrame,
                                       std::uint32_t requestColor,
                                       std::uint32_t currentColor) noexcept {
    if (phase < 0 || phase > 4 || requestFrame > now) return false;
    if (phase == 0) return true;
    if (requestFrame != now) return false;
    return phase != 3 || requestColor == currentColor;
}

}
