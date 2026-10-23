#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace bl::csharp {

struct NativeTooltipPanelLine {
    float width=0,height=0;
};

struct NativeTooltipPanelRequest {
    int screenWidth=0,screenHeight=0;
    float mouseX=0,mouseY=0;
    float overloadedX=-1,overloadedY=-1;
    float distanceX=0,distanceY=0;
    float drawOffsetX=0,drawOffsetY=0;
    float scale=1;
    float padX=0,padY=0;
    bool opaque=false;
    bool bannerVisible=false;
    float bannerBottom=0;
};

struct NativeTooltipPanelResult {
    float textX=0,textY=0;
    float panelX=0,panelY=0,panelWidth=0,panelHeight=0;
    float contentWidth=0,contentHeight=0;
    bool right=true;
    bool opaque=false;
};

inline bool nativeTooltipPanelMeasure(std::span<const NativeTooltipPanelLine> lines,
                                      float* width,float* height) {
    if(!width||!height||lines.empty()||lines.size()>64)return false;
    float maxWidth=0,totalHeight=0;
    for(const auto& line:lines) {
        if(!std::isfinite(line.width)||!std::isfinite(line.height)||
           line.width<0||line.height<=0||line.width>16000||line.height>2048)
            return false;
        maxWidth=std::max(maxWidth,line.width);
        totalHeight+=line.height;
        if(!std::isfinite(totalHeight)||totalHeight>16000)return false;
    }
    *width=maxWidth;*height=totalHeight;
    return true;
}

inline bool nativeTooltipPanelInsertLine(std::span<const NativeTooltipPanelLine> measured,
                                         size_t index,NativeTooltipPanelLine inserted,
                                         std::vector<NativeTooltipPanelLine>* out) {
    if(!out||index>measured.size()||measured.empty()||measured.size()>=64)return false;
    std::vector<NativeTooltipPanelLine> staged(measured.begin(),measured.end());
    staged.insert(staged.begin()+static_cast<std::ptrdiff_t>(index),inserted);
    float width=0,height=0;
    if(!nativeTooltipPanelMeasure(staged,&width,&height))return false;
    *out=std::move(staged);
    return true;
}

inline bool nativeTooltipPanelLayout(const NativeTooltipPanelRequest& request,
                                    std::span<const NativeTooltipPanelLine> lines,
                                    NativeTooltipPanelResult* result) {
    if(!result||request.screenWidth<320||request.screenHeight<320||
       request.screenWidth>16000||request.screenHeight>16000)return false;
    for(const float value:{request.mouseX,request.mouseY,request.overloadedX,
                           request.overloadedY,request.distanceX,request.distanceY,
                           request.drawOffsetX,request.drawOffsetY,request.scale,
                           request.padX,request.padY,request.bannerBottom})
        if(!std::isfinite(value))return false;
    if(request.scale<0.5f||request.scale>2.5f||
       request.distanceX<0||request.distanceY<0||
       request.padX<0||request.padY<0||request.padX>256||request.padY>256||
       request.mouseX<0||request.mouseY<0)return false;
    if(request.bannerVisible&&
       (request.bannerBottom<0||request.bannerBottom>request.screenHeight))
        return false;
    float width=0,height=0;
    if(!nativeTooltipPanelMeasure(lines,&width,&height))return false;
    const float padX=request.opaque?request.padX:0.f;
    const float padY=request.opaque?request.padY:0.f;
    if(width+2*padX+request.distanceX>request.screenWidth||
       height+2*padY+request.distanceY>request.screenHeight)return false;
    const float middle=request.screenWidth>889?
        static_cast<float>(request.screenWidth)/2.f:445.f;
    const bool overridePosition=request.overloadedX!=-1;
    bool right=true;
    float x=0,y=0;
    if(overridePosition) {
        if(request.overloadedX<0||request.overloadedY<0)return false;
        x=request.overloadedX;
        y=request.overloadedY+std::trunc(request.drawOffsetY);
    }else {
        right=request.mouseX<middle;
        x=request.mouseX+(right?1.f:-1.f)*std::trunc(request.drawOffsetX);
        y=request.mouseY+std::trunc(request.drawOffsetY);
    }
    if(request.distanceX>x)x=std::trunc(request.distanceX);
    if(request.distanceY>y)y=std::trunc(request.distanceY);
    if(right) {
        if(x+width+request.distanceX+padX*2>request.screenWidth)
            x=std::trunc(request.screenWidth-width-request.distanceX-padX*2);
    }else if(x-width-request.distanceX-padX*2<0) {
        x=std::trunc(padX*2)+std::trunc(request.distanceX)+std::trunc(width);
    }
    if(y+height+request.distanceY>request.screenHeight)
        y=std::trunc(request.screenHeight-height-request.distanceY);
    if(request.bannerVisible) {
        const float overlap=padY+request.distanceY+height+y-request.bannerBottom;
        if(overlap>0)y=overlap>y?0:y-std::trunc(overlap);
    }
    const float panelX=right?x:x-width;
    if(!std::isfinite(x)||!std::isfinite(y)||
       panelX<0||y<0||panelX+width+2*padX>request.screenWidth||
       y+height+2*padY>request.screenHeight)return false;
    NativeTooltipPanelResult next;
    next.textX=x;next.textY=y;
    next.panelX=panelX;next.panelY=y;
    next.panelWidth=width+2*padX;next.panelHeight=height+2*padY;
    next.contentWidth=width;next.contentHeight=height;
    next.right=right;next.opaque=request.opaque;
    *result=next;
    return true;
}

}
