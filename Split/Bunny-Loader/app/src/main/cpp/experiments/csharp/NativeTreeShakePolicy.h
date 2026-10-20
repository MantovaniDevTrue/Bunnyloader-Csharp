#pragma once
#include <cstddef>
#include <cstdint>
namespace bl::csharp::tree_shake {
inline bool validItem(int item) { return item>=6196&&item<65536; }
inline bool canShake(int x,int y,int width,int height,int shakes,int maximum,int netMode) {
    return x>=2&&x<width-2&&y>=12&&y<height-2&&
           maximum>0&&maximum<=4096&&shakes>=0&&shakes<maximum&&
           netMode!=1&&(netMode==0||netMode==2);
}
inline bool repeated(const int32_t* xs,const int32_t* ys,std::size_t capacity,
                     int count,int x,int groundY) {
    if(!xs||!ys||count<0||static_cast<std::size_t>(count)>capacity)return true;
    for(int i=0;i<count;++i)if(xs[i]==x&&ys[i]==groundY)return true;
    return false;
}
inline bool sendLeaf(int leaf,bool enabled,int netMode) {
    return enabled&&leaf>=0&&leaf<32768&&(netMode==0||netMode==2);
}
}
