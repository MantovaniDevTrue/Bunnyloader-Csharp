#pragma once
#include <cstdint>

namespace bl::csharp::tile_policy {
inline constexpr int kSupportedManagedCallbacks = (1<<21)-1;
inline constexpr int kManagedDrawCallbacks = (7<<13)|(1<<5)|(15<<16);
inline bool validCallbackMask(int mask) { return mask >= 0 && (mask & ~kSupportedManagedCallbacks) == 0; }


inline int leftOf2x1(int x, int frameX) {
    if (frameX < 0) return x;
    return x - ((frameX % 36) / 18);
}

struct TileFootprint {
    int width=0, height=0, frameWidth=0, frameHeight=0;
};
inline TileFootprint footprint(int shape) {
    switch(shape) {
        case 1: return {2,1,36,18};
        case 2: return {3,2,54,36};
        case 3: return {1,3,18,54};
        default: return {};
    }
}
inline bool validShape(int shape) {return shape>=0&&shape<=3;}
inline bool origin(int shape,int x,int y,int frameX,int frameY,int* left,int* top) {
    const auto size=footprint(shape);
    if(size.width==0||frameX<0||frameY<0||!left||!top)return false;
    const int dx=(frameX%size.frameWidth)/18;
    const int dy=(frameY%size.frameHeight)/18;
    if(x<dx||y<dy)return false;
    *left=x-dx;*top=y-dy;return true;
}
inline bool frameMatches(int shape,int frameX,int frameY,int anchorX,int anchorY,
                         int dx,int dy) {
    const auto size=footprint(shape);
    if(size.width==0||dx<0||dy<0||dx>=size.width||dy>=size.height||
       frameX<0||frameY<0||anchorX<0||anchorY<0)return false;
    return frameX%size.frameWidth==dx*18 &&
           frameY%size.frameHeight==dy*18 &&
           frameX/size.frameWidth==anchorX/size.frameWidth &&
           frameY/size.frameHeight==anchorY/size.frameHeight;
}

inline bool shouldDrop(int shape, bool includeLargeObjectDrops) {
    return validShape(shape)&&(shape == 0 || includeLargeObjectDrops);
}

inline bool shouldDrop(int shape, bool large, bool allowDrops) {
    return allowDrops && shouldDrop(shape, large);
}

inline bool blocksCanKillTile(bool allowBreaking) {
    return !allowBreaking;
}

inline int dustBudget(bool fail, int failCount, int normalCount) {
    return fail ? failCount : normalCount;
}

inline bool incomplete2x1(bool left, bool right, bool leftSupport, bool rightSupport) {
    return !left || !right || !leftSupport || !rightSupport;
}

}
