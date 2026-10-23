#pragma once
#include "experiments/csharp/NativeTooltipPlan.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp {

struct NativeTooltipWaveLayout {
    std::u16string units;
    std::vector<float> prefixWidths;
};

struct NativeTooltipPaint {
    float x=0,y=0;
    float scale=1,spread=1;
    float rotation=0,originX=0,originY=0,depth=0;
    float clock=0;
    uint8_t opacity=255;
};

inline bool nativeTooltipUtf16(std::string_view text,std::u16string* result) {
    if(!result||text.size()>512)return false;
    std::u16string decoded;decoded.reserve(text.size());
    for(size_t i=0;i<text.size();) {
        const unsigned char first=static_cast<unsigned char>(text[i++]);
        uint32_t cp=first;int following=0;uint32_t minimum=0;
        if(first<0x80){}
        else if(first>=0xC2&&first<=0xDF){cp=first&31;following=1;minimum=0x80;}
        else if(first>=0xE0&&first<=0xEF){cp=first&15;following=2;minimum=0x800;}
        else if(first>=0xF0&&first<=0xF4){cp=first&7;following=3;minimum=0x10000;}
        else return false;
        if(i+static_cast<size_t>(following)>text.size())return false;
        while(following--) {
            const unsigned char next=static_cast<unsigned char>(text[i++]);
            if((next&0xC0)!=0x80)return false;
            cp=(cp<<6)|(next&63);
        }
        if(cp<minimum||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||cp==0||cp==127)return false;
        if(cp<0x10000)decoded.push_back(static_cast<char16_t>(cp));
        else {
            cp-=0x10000;
            decoded.push_back(static_cast<char16_t>(0xD800+(cp>>10)));
            decoded.push_back(static_cast<char16_t>(0xDC00+(cp&1023)));
        }
        if(decoded.size()>512)return false;
    }
    *result=std::move(decoded);return true;
}

template<class Measure>
bool buildNativeTooltipWaveLayout(std::string_view text,Measure&& measure,NativeTooltipWaveLayout* result) {
    if(!result)return false;
    NativeTooltipWaveLayout built;
    if(!nativeTooltipUtf16(text,&built.units))return false;
    built.prefixWidths.reserve(built.units.size());
    for(size_t i=0;i<built.units.size();++i) {
        const float width=i?measure(std::u16string_view(built.units.data(),i)):0.f;
        if(!std::isfinite(width)||width<0.f||width>100000.f)return false;
        built.prefixWidths.push_back(width);
    }
    *result=std::move(built);return true;
}

template<class Paint>
bool drawNativeTooltipWave(const NativeTooltipLine& line,const NativeTooltipWaveLayout& layout,
                          const NativeTooltipPaint& params,Paint&& paint) {
    if(!line.rainbow||layout.units.size()!=layout.prefixWidths.size()||
       layout.units.size()>512||!std::isfinite(params.x)||!std::isfinite(params.y)||
       !std::isfinite(params.scale)||params.scale<=0||params.scale>4||
       !std::isfinite(params.spread)||params.spread<0||params.spread>8||
       !std::isfinite(params.rotation)||!std::isfinite(params.originX)||
       !std::isfinite(params.originY)||!std::isfinite(params.depth)||
       !std::isfinite(params.clock))return false;
    for(size_t i=0;i<layout.units.size();++i) {
        const char16_t ch=layout.units[i];
        if(ch==u' ')continue;
        if(!std::isfinite(layout.prefixWidths[i])||layout.prefixWidths[i]<0.f)return false;
        const auto glyph=nativeTooltipGlyph(line,params.clock,i);
        const float x=params.x+layout.prefixWidths[i]*params.scale;
        const float y=params.y+glyph.yOffset;
        const uint32_t foreground=(static_cast<uint32_t>(params.opacity)<<24)|
            (static_cast<uint32_t>(glyph.r)<<16)|(static_cast<uint32_t>(glyph.g)<<8)|glyph.b;
        const uint32_t shadow=static_cast<uint32_t>(params.opacity)<<24;
        for(int pass=0;pass<5;++pass) {
            const float dx=pass==0?-params.spread:pass==1?params.spread:0.f;
            const float dy=pass==2?-params.spread:pass==3?params.spread:0.f;
            paint(ch,x+dx,y+dy,pass==4?foreground:shadow,params,pass);
        }
    }
    return true;
}

}
