#pragma once
#include <algorithm>
#include <cmath>
namespace bl::csharp::tile_campfire {
struct Bounds {int left=0,top=0,right=0,bottom=0;};
inline Bounds area(float centerX,float centerY,int scanWidth,int scanHeight,int worldWidth,int worldHeight) {
    if(!std::isfinite(centerX)||!std::isfinite(centerY)||
       std::abs(centerX)>10000000.f||std::abs(centerY)>10000000.f||
       scanWidth<1||scanWidth>256||scanHeight<1||scanHeight>256||
       worldWidth<1||worldWidth>100000||worldHeight<1||worldHeight>100000)return {};
    const int x=static_cast<int>(std::floor(centerX/16.f));
    const int y=static_cast<int>(std::floor(centerY/16.f));
    const int x0=std::clamp(x-scanWidth/2,0,worldWidth);
    const int y0=std::clamp(y-scanHeight/2,0,worldHeight);
    const int x1=std::clamp(x+scanWidth/2,0,worldWidth);
    const int y1=std::clamp(y+scanHeight/2,0,worldHeight);
    return {x0,y0,std::max(x0,x1),std::max(y0,y1)};
}
inline bool lit(bool active,int frameY) {return active&&frameY>=0&&frameY<36;}
inline bool soundPosition(int x,int y,float* outX,float* outY) {
    if(!outX||!outY||x<0||y<0||x>100000||y>100000)return false;
    *outX=static_cast<float>(x)*16.f;
    *outY=static_cast<float>(y)*16.f;
    return true;
}
}
