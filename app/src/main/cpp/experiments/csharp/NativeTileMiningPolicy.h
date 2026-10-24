#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace bl::csharp::tile_mining {
inline bool valid(int minPick,float resist) {
    return minPick>=0&&minPick<=1000&&std::isfinite(resist)&&resist>=0.f&&resist<=128.f;
}
inline int apply(int damage,int pickPower,int minPick,float resist) {
    if(pickPower<minPick)return 0;
    if(damage<=0||resist<=0.f)return damage;
    const double reduced=std::floor(static_cast<double>(damage)/static_cast<double>(resist));
    if(reduced>=static_cast<double>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    return static_cast<int>(reduced);
}
}
