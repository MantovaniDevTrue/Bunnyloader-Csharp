#pragma once

#include <cstdint>

namespace bl::il2cpp {

constexpr bool plausibleManagedPointerValue(std::uintptr_t value, bool taggedArm64) {
    if (taggedArm64) {
        value &= UINT64_C(0x00FFFFFFFFFFFFFF);
        if (value >= UINT64_C(0x0001000000000000)) return false;
    }
    return value >= static_cast<std::uintptr_t>(0x10000) &&
           (value & (alignof(void*) - 1)) == 0;
}

inline bool nativeManagedPointerPlausible(const void* pointer) {
#if defined(__aarch64__)
    return plausibleManagedPointerValue(
        reinterpret_cast<std::uintptr_t>(pointer), true);
#else
    return plausibleManagedPointerValue(
        reinterpret_cast<std::uintptr_t>(pointer), false);
#endif
}

}
