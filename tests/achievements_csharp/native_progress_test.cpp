#include "experiments/csharp/NativeAchievementProgress.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using bl::csharp::nativeAchievementProgress;
    constexpr int max = bl::csharp::kNativeAchievementTrackerMaximum;
    assert(nativeAchievementProgress(0, false) == 0);
    assert(nativeAchievementProgress(0.2, false) == 200000);
    assert(nativeAchievementProgress(0.5, false) == 500000);
    assert(nativeAchievementProgress(0.75, false) == 750000);
    assert(nativeAchievementProgress(1, true) == max);
    assert(nativeAchievementProgress(1, false) == max);
    assert(nativeAchievementProgress(0.000001, false) == 1);
    assert(nativeAchievementProgress(0, true) == max);
    assert(nativeAchievementProgress(-1, false) == 0);
    assert(nativeAchievementProgress(2, false) == max);
    assert(nativeAchievementProgress(std::numeric_limits<double>::quiet_NaN(), false) == 0);
    assert(nativeAchievementProgress(std::numeric_limits<double>::infinity(), false) == 0);
    std::cout << "CSHARP_ACHIEVEMENT_NATIVE_PROGRESS_PASS=YES\n";
}
