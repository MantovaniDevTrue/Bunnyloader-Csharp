#pragma once
#include "experiments/csharp/NativeTileSpriteWire.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace bl::csharp::tile_particle {

inline constexpr size_t kMaxCellParticles=2;
inline constexpr size_t kMaxFrameParticles=24;

struct Particle {
    int type=0,x=0,y=0,dustType=0,offsetX=0,offsetY=0;
    int width=0,height=0,speedXMilli=0,speedYMilli=0,alpha=0,scaleMilli=1000;
    uint32_t argb=0xffffffffu;
};

struct Batch {
    std::array<Particle,kMaxCellParticles> items{};
    size_t count=0;
};

inline bool valid(const Particle& p) {
    return p.type>=754 && p.type<=65535 && p.x>=0 && p.x<=100000 &&
           p.y>=0 && p.y<=100000 && p.dustType>=0 && p.dustType<=4095 &&
           p.offsetX>=-64 && p.offsetX<=64 && p.offsetY>=-64 && p.offsetY<=64 &&
           p.width>=1 && p.width<=32 && p.height>=1 && p.height<=32 &&
           p.speedXMilli>=-10000 && p.speedXMilli<=10000 &&
           p.speedYMilli>=-10000 && p.speedYMilli<=10000 &&
           p.alpha>=0 && p.alpha<=255 && p.scaleMilli>=100 && p.scaleMilli<=3000;
}

inline bool parseItem(std::string_view value,Particle* out) {
    if(!out)return false;
    std::array<int32_t,12> nums{};
    size_t from=0;
    for(size_t i=0;i<nums.size();++i) {
        size_t comma=value.find(',',from);
        if(comma==std::string_view::npos ||
           !tile_sprite::integer(value.substr(from,comma-from),&nums[i]))
            return false;
        from=comma+1;
    }
    if(value.find(',',from)!=std::string_view::npos)return false;
    uint32_t color=0;
    if(!tile_sprite::color(value.substr(from),&color))return false;
    Particle candidate{nums[0],nums[1],nums[2],nums[3],nums[4],nums[5],
                       nums[6],nums[7],nums[8],nums[9],nums[10],nums[11],color};
    if(!valid(candidate))return false;
    *out=candidate;
    return true;
}

inline bool parse(std::string_view text,Batch* out) {
    if(!out||text.size()>512||!text.starts_with("PARTICLES|"))return false;
    const size_t end=text.find('|',10);
    int32_t count=0;
    if(end==std::string_view::npos ||
       !tile_sprite::integer(text.substr(10,end-10),&count) ||
       count<1||count>static_cast<int32_t>(kMaxCellParticles))return false;
    Batch result{};
    size_t from=end+1;
    for(int i=0;i<count;++i) {
        size_t sep=text.find('|',from);
        if((i+1<count&&sep==std::string_view::npos)||
           (i+1==count&&sep!=std::string_view::npos))return false;
        const size_t last=sep==std::string_view::npos?text.size():sep;
        if(last<=from||!parseItem(text.substr(from,last-from),&result.items[i]))
            return false;
        ++result.count;
        from=last+1;
    }
    *out=result;
    return true;
}
inline bool owned(const Batch& batch,int type,int x,int y) {
    if(batch.count==0||batch.count>kMaxCellParticles)return false;
    for(size_t i=0;i<batch.count;++i)
        if(!valid(batch.items[i])||batch.items[i].type!=type||
           batch.items[i].x!=x||batch.items[i].y!=y)return false;
    return true;
}
}
