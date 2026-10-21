#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {

constexpr int kNativeAchievementTrackerMaximum = 1000000;

// O progresso gerenciado continua sendo a fonte da verdade.
// O tracker nativo arredonda frações para int apenas para apresentação.
inline int nativeAchievementProgress(double fraction, bool completed) {
    if (completed) return kNativeAchievementTrackerMaximum;
    if (!std::isfinite(fraction) || fraction <= 0) return 0;
    if (fraction >= 1) return kNativeAchievementTrackerMaximum;
    return static_cast<int>(std::clamp(
        std::llround(fraction * kNativeAchievementTrackerMaximum),
        0LL, static_cast<long long>(kNativeAchievementTrackerMaximum)));
}

}
