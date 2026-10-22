#pragma once

#include <array>
#include <cstdint>

namespace bl::csharp::banners {

struct BannerAnchorPlan {
    int32_t height;
    int32_t coordinateWidth;
    std::array<int32_t,3> coordinateHeights;
    int32_t solidDrawYOffset;
    int32_t platformDrawYOffset;
    int32_t platformAlternateIndex;
    int32_t anchorCheckStart;
};

inline constexpr BannerAnchorPlan kBannerAnchorPlan{
    3, 1, {16,16,16}, -2, -10, 0, 0
};

inline bool bannerAnchorMaskValid(const std::array<int32_t,6>& flags) {
    for (int32_t flag : flags)
        if (flag <= 0) return false;
    const int32_t solid=flags[0]|flags[1]|flags[2]|flags[3];
    const int32_t platform=flags[4]|flags[5];
    return (solid & platform)==0 && solid!=platform;
}

} // namespace bl::csharp::banners
