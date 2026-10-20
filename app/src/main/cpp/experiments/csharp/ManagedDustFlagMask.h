#pragma once

#include <cstdint>

namespace bl::csharp {

inline bool managedDustFlagsValid(int phase, std::uint32_t flags,
                                  bool active) noexcept {
    if (phase < 0 || phase > 5) return false;
    if ((flags & 1u) == 0u) return false;
    if (((flags & 32u) != 0u) != active) return false;
    constexpr std::uint32_t allowed[] = {
        1u | 32u,
        1u | 2u | 16u | 32u,
        1u | 2u | 4u | 16u | 32u,
        1u | 2u | 8u | 16u | 32u,
        1u | 2u | 16u | 32u,
        1u
    };
    return (flags & ~allowed[phase]) == 0u;
}

}
