#include "experiments/csharp/ManagedBannerAnchorPolicy.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::banners;
    static_assert(kBannerAnchorPlan.height==3);
    static_assert(kBannerAnchorPlan.coordinateWidth==1);
    static_assert(kBannerAnchorPlan.coordinateHeights[0]==16);
    static_assert(kBannerAnchorPlan.coordinateHeights[1]==16);
    static_assert(kBannerAnchorPlan.coordinateHeights[2]==16);
    static_assert(kBannerAnchorPlan.solidDrawYOffset==-2);
    static_assert(kBannerAnchorPlan.platformDrawYOffset==-10);
    static_assert(kBannerAnchorPlan.platformAlternateIndex==0);
    static_assert(kBannerAnchorPlan.anchorCheckStart==0);
    assert(bannerAnchorMaskValid({1,2,4,8,16,32}));
    assert(!bannerAnchorMaskValid({1,2,4,8,0,32}));
    assert(!bannerAnchorMaskValid({1,2,4,8,1,32}));
    assert(!bannerAnchorMaskValid({1,2,4,8,4,8}));
    std::cout << "Banner anchor fidelity PASS (solid, platform, height, offsets)\n";
}
