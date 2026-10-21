#pragma once
#include "experiments/csharp/NativeTilePolicy.h"
#include <array>
namespace bl::csharp::tile_toggle {
struct Patch {int x=0,y=0,fromX=0,fromY=0,toX=0,toY=0;};
struct Plan {
    std::array<Patch,6> cells{};
    int count=0,left=0,top=0,width=0,height=0;
};
inline bool allowsRightClick(int mode) {return mode==1;}
inline bool plan(int shape,int mode,int x,int y,int frameX,int frameY,Plan* out) {
    if(!out||mode<1||mode>2||(mode==1&&shape!=2)||(mode==2&&shape!=3))
        return false;
    const auto size=tile_policy::footprint(shape);
    if(size.width==0)return false;
    int left=0,top=0;
    if(!tile_policy::origin(shape,x,y,frameX,frameY,&left,&top))return false;
    const int baseX=frameX-(x-left)*18,baseY=frameY-(y-top)*18;
    const int base=mode==1?baseY:baseX;
    const int step=mode==1?36:18;
    if(base!=0&&base!=step)return false;
    const int shift=base==0?step:-step;
    Plan result{};
    result.left=left;result.top=top;result.width=size.width;result.height=size.height;
    for(int dy=0;dy<size.height;++dy)for(int dx=0;dx<size.width;++dx) {
        auto& cell=result.cells[result.count++];
        cell.x=left+dx;cell.y=top+dy;
        cell.fromX=baseX+dx*18;cell.fromY=baseY+dy*18;
        cell.toX=cell.fromX+(mode==2?shift:0);
        cell.toY=cell.fromY+(mode==1?shift:0);
    }
    *out=result;
    return true;
}
inline bool lit(int mode,int frameX,int frameY) {
    if(mode==0)return true;
    if(mode==1)return frameY>=0&&frameY<36;
    if(mode==2)return frameX==0;
    return false;
}
}
