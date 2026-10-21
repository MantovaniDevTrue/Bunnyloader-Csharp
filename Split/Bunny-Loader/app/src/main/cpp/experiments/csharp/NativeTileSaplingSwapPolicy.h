#pragma once
#include "experiments/csharp/NativeTilePolicy.h"

namespace bl::csharp::tile_sapling {
struct SwapPlan {
    int x=0, top=0, bottom=0, sourceType=0;
};
inline bool swapPlan(int x,int y,int type,int frameX,int frameY,
                     int width,int height,SwapPlan* out) {
    if(!out||type<754||type>=65536||width<=0||height<=0||
       x<0||x>=width||y<0||y>=height)return false;
    int left=0,top=0;
    if(!tile_policy::origin(4,x,y,frameX,frameY,&left,&top)||
       left!=x||top<0||top>=height-1)return false;
    *out={x,top,top+1,type};
    return true;
}
inline bool matchingFrames(const SwapPlan& plan,int anchorX,int anchorY,
                           int topX,int topY,int bottomX,int bottomY) {
    return tile_policy::frameMatches(4,topX,topY,anchorX,anchorY,0,0)&&
           tile_policy::frameMatches(4,bottomX,bottomY,anchorX,anchorY,0,1)&&
           plan.bottom==plan.top+1;
}
}
