#pragma once

#include <cmath>

namespace bl::csharp {
inline bool validNativeWeaponShootSpeed(float value, bool summon) {
    return std::isfinite(value) &&
           value >= (summon ? 0.0f : 1.0f) &&
           value <= 40.0f;
}
}
