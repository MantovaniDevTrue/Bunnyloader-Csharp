#pragma once
#include "experiments/csharp/NativeTileParticleWire.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace bl::csharp::tile_particle {
inline bool safeFieldOffset(size_t offset,size_t width,size_t instanceBytes) {
    return instanceBytes>=sizeof(void*)*2 && instanceBytes<=8192 &&
           offset>=sizeof(void*)*2 && width<=instanceBytes &&
           offset<=instanceBytes-width;
}
inline bool applyMotion(const Particle& input,float oldX,float oldY,
                        float* updatedX,float* updatedY) {
    if(!updatedX||!updatedY||!std::isfinite(oldX)||!std::isfinite(oldY)||
       !valid(input))return false;
    const float newX=oldX*static_cast<float>(input.velocityXScaleMilli)*0.001f;
    const float newY=oldY*static_cast<float>(input.velocityYScaleMilli)*0.001f+
                     static_cast<float>(input.velocityYAddMilli)*0.001f;
    if(!std::isfinite(newX)||!std::isfinite(newY)||
       std::abs(newX)>2000.f||std::abs(newY)>2000.f)return false;
    *updatedX=newX;
    *updatedY=newY;
    return true;
}
inline int saturatingAlpha(int original,int extra) {
    return std::clamp(original+extra,0,255);
}
}
