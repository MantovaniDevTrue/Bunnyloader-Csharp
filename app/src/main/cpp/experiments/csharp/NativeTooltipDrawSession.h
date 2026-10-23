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

    bool begin(const NativeTooltipPlan& plan) {
        animated.reset();anchor.clear();inserted=false;shift=0;
        tooltipRgb=0;tooltipTint=false;
        if(plan.lines.size()!=3||plan.lines[0].name!="ItemName"||
           plan.lines[1].name!="BunnyLoader"||!plan.lines[1].rainbow||
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

    template<class Insert>
    float adjust(std::u16string_view text,float y,float lineHeight,Insert&& insert) {
        if(inserted)return std::isfinite(y)?y+shift:y;
        if(!animated||!std::isfinite(y)||!std::isfinite(lineHeight)||
           lineHeight<=0||lineHeight>100)return y;
        if(matches(text)) {
            if(!insert(*animated,y))return y;
            shift=lineHeight;
            inserted=true;
        }
        return inserted?y+shift:y;
    }
};

}
