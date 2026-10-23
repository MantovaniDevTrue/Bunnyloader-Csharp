#pragma once
#include "experiments/csharp/NativeTooltipWave.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bl::csharp {

struct NativeTooltipDrawSession {
    std::optional<NativeTooltipLine> animated;
    std::u16string anchor;
    bool inserted=false;
    float shift=0;
    uint32_t tooltipRgb=0;
    bool tooltipTint=false;
    uintptr_t batchToken=0,fontToken=0;
    float anchorX=0,anchorY=0;

    bool begin(const NativeTooltipPlan& plan) {
        animated.reset();anchor.clear();inserted=false;shift=0;
        tooltipRgb=0;tooltipTint=false;
        batchToken=0;fontToken=0;anchorX=0;anchorY=0;
        if(plan.lines.size()!=3||plan.lines[0].name!="ItemName"||
           plan.lines[1].name!="BunnyLoader"||!plan.lines[1].rainbow||
           plan.lines[1].draw||
           plan.lines[2].name!="Tooltip0"||plan.lines[2].text.empty())return false;
        if(!nativeTooltipUtf16(plan.lines[2].text,&anchor)||anchor.empty()||anchor.size()>512)
            return false;
        const auto& color=plan.lines[2].color;
        if(color.size()==6) {
            uint32_t rgb=0;
            for(char ch:color) {
                const int digit=ch>='0'&&ch<='9'?ch-'0':
                    ch>='A'&&ch<='F'?ch-'A'+10:
                    ch>='a'&&ch<='f'?ch-'a'+10:-1;
                if(digit<0)return false;
                rgb=(rgb<<4)|static_cast<uint32_t>(digit);
            }
            tooltipRgb=rgb;tooltipTint=true;
        }
        animated=plan.lines[1];
        return true;
    }

    bool matches(std::u16string_view text) const {
        return animated&&text==std::u16string_view(anchor);
    }

    bool inRegion(uintptr_t batch,uintptr_t font,float x,float y) const {
        return inserted&&batch!=0&&font!=0&&batch==batchToken&&font==fontToken&&
            std::isfinite(x)&&std::isfinite(y)&&
            std::abs(x-anchorX)<=64.f&&
            y>=anchorY-4.f&&y<=anchorY+320.f;
    }

    bool isAnchorDraw(std::u16string_view text,uintptr_t batch,uintptr_t font,
                      float x,float y) const {
        return matches(text)&&inRegion(batch,font,x,y-shift)&&
            std::abs(x-anchorX)<=4.f&&std::abs(y-(anchorY+shift))<=4.f;
    }

    template<class Insert>
    float adjust(std::u16string_view text,uintptr_t batch,uintptr_t font,
                 float x,float y,float lineHeight,Insert&& insert) {
        if(inserted)return inRegion(batch,font,x,y)?y+shift:y;
        if(!animated||!batch||!font||!std::isfinite(x)||!std::isfinite(y)||
           !std::isfinite(lineHeight)||lineHeight<=0||lineHeight>100)return y;
        if(matches(text)) {
            if(!insert(*animated,y))return y;
            anchorX=x;anchorY=y;batchToken=batch;fontToken=font;
            shift=lineHeight;
            inserted=true;
        }
        return inserted?y+shift:y;
    }
};

}
