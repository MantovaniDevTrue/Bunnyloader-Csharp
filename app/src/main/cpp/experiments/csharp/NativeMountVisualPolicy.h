#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace bl::csharp::mount_visual {

struct Point {
    float x=0,y=0;
};

struct Balloon {
    float rotation=0;
    Point position{},origin{};
    int frame=0;
};

inline bool updateBalloons(std::array<float,3>* rotations,float velocityX,
                           const std::array<float,3>& random,int count) {
    if(!rotations||count!=3||!std::isfinite(velocityX))return false;
    float scale=0.05f;
    constexpr float halfPi=1.57079632679489661923f;
    for(int i=0;i<count;++i) {
        float& rotation=(*rotations)[static_cast<size_t>(i)];
        const float sample=random[static_cast<size_t>(i)];
        if(!std::isfinite(rotation)||!std::isfinite(sample)||sample<0||sample>1)
            return false;
        if(std::abs(rotation)>halfPi)scale=-scale;
        const float next=rotation-velocityX*scale*sample;
        rotation=next+(0.f-next)*0.05f;
    }
    return true;
}

inline Balloon balloonAt(int i,int direction,int64_t milliseconds,
                         Point draw,float mountRotation,float balloonRotation) {
    constexpr std::array<int,3> offsets{0,14,-14};
    Balloon b;
    if(i<0||i>=3||direction==0||!std::isfinite(draw.x)||!std::isfinite(draw.y)||
       !std::isfinite(mountRotation)||!std::isfinite(balloonRotation))return b;
    const int facing=direction<0?-1:1;
    const int64_t timer=((milliseconds%800)+800)%800/200;
    b.position={draw.x+static_cast<float>((-36+offsets[static_cast<size_t>(i)])*facing),draw.y+14.f};
    b.origin={static_cast<float>(14+facing*7),42.f};
    b.rotation=mountRotation+balloonRotation;
    b.frame=static_cast<int>((timer+i)%4);
    return b;
}

inline Point ringDust(Point center,int i) {
    if(i<0||i>=16||!std::isfinite(center.x)||!std::isfinite(center.y))
        return center;
    constexpr double tau=6.283185307179586476925286766559;
    const double angle=static_cast<double>(i)*tau/16.0;
    return {center.x+static_cast<float>(80.0*std::cos(angle)),
            center.y+static_cast<float>(80.0*std::sin(angle))};
}

inline bool minecartSparkle(uint32_t sample) {
    return sample%10u==0u;
}

}
