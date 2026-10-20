#pragma once
#include <algorithm>
#include <cstdint>
namespace bl::csharp::tile_smart {
struct Bounds {int left=0,top=0,right=0,bottom=0;};
inline bool bounds(int lx,int ly,int hx,int hy,int worldWidth,int worldHeight,Bounds* out) {
    if(!out||worldWidth<=0||worldHeight<=0||worldWidth>100000||worldHeight>100000||
       hx<lx||hy<ly)return false;
    const int64_t width=static_cast<int64_t>(hx)-lx+1;
    const int64_t height=static_cast<int64_t>(hy)-ly+1;
    if(width<=0||height<=0||width>64||height>64||width*height>4096)return false;
    Bounds b{std::clamp(lx,0,worldWidth),std::clamp(ly,0,worldHeight),
             static_cast<int>(std::clamp(static_cast<int64_t>(hx)+1,static_cast<int64_t>(0),static_cast<int64_t>(worldWidth))),
             static_cast<int>(std::clamp(static_cast<int64_t>(hy)+1,static_cast<int64_t>(0),static_cast<int64_t>(worldHeight)))};
    if(b.right<=b.left||b.bottom<=b.top)return false;
    *out=b;return true;
}
inline bool eligible(int callbackMask,int frameToggle) {
    return (callbackMask&(1<<4))!=0||frameToggle==1;
}
}
