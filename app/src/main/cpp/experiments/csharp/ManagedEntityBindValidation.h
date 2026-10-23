#pragma once

#include <cstdint>
#include <cstring>

namespace bl::csharp {

inline bool managedEntityBindAccepted(const void* unboxedResult) {
    if (!unboxedResult) return false;
    uint8_t accepted = 0;
    std::memcpy(&accepted, unboxedResult, sizeof(accepted));
    return accepted == 1;
}

}
