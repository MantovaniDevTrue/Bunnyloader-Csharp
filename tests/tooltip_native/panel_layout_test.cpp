#include "experiments/csharp/NativeTooltipPanelLayout.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace bl::csharp;

int main() {
    std::vector<NativeTooltipPanelLine> original{{120,20},{155,30},{80,20}};
    std::vector<NativeTooltipPanelLine> withCSharp;
    assert(nativeTooltipPanelInsertLine(original,1,{135,23},&withCSharp));
    assert(withCSharp.size()==4&&original.size()==3);
    float w=0,h=0;
    assert(nativeTooltipPanelMeasure(original,&w,&h)&&w==155&&h==70);
    assert(nativeTooltipPanelMeasure(withCSharp,&w,&h)&&w==155&&h==93);
    NativeTooltipPanelRequest input;
    input.screenWidth=1080;input.screenHeight=1920;
    input.mouseX=50;input.mouseY=200;
    input.distanceX=12;input.distanceY=10;
    input.drawOffsetX=30;input.drawOffsetY=20;
    input.scale=1;input.opaque=true;input.padX=8;input.padY=5;
    NativeTooltipPanelResult panel;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.right&&panel.opaque&&panel.textX==80&&panel.textY==220);
    assert(panel.panelX==80&&panel.panelY==220);
    assert(panel.panelWidth==171&&panel.panelHeight==103);
    assert(panel.contentWidth==155&&panel.contentHeight==93);

    input.mouseX=950;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(!panel.right&&panel.textX==920&&panel.panelX==765);
    assert(panel.panelWidth==171);

    input.mouseX=50;input.mouseY=1875;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.textY==1817&&panel.panelY+panel.panelHeight<=1920);

    input.mouseY=200;input.overloadedX=1060;input.overloadedY=1600;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.right&&panel.textX==897&&panel.textY==1620);
    input.overloadedX=-1;input.overloadedY=-1;

    input.bannerVisible=true;input.bannerBottom=260;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.panelY==152&&panel.textY==152);
    input.bannerBottom=85;
    NativeTooltipPanelResult unchanged;
    unchanged.textX=999;
    assert(!nativeTooltipPanelLayout(input,withCSharp,&unchanged));
    assert(unchanged.textX==999);
    input.bannerVisible=false;
    input.bannerBottom=0;

    input.opaque=false;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.panelWidth==155&&panel.panelHeight==93);
    input.opaque=true;

    input.screenHeight=320;input.mouseY=230;
    assert(nativeTooltipPanelLayout(input,withCSharp,&panel));
    assert(panel.textY==217);
    input.screenHeight=1920;input.mouseY=200;

    NativeTooltipPanelResult sentinel;
    sentinel.textX=4444;
    auto reject=[&](const NativeTooltipPanelRequest& request) {
        assert(!nativeTooltipPanelLayout(request,withCSharp,&sentinel));
        assert(sentinel.textX==4444);
    };
    auto bad=input;
    bad.screenHeight=200;reject(bad);
    bad=input;bad.scale=4;reject(bad);
    bad=input;bad.padX=999;reject(bad);
    bad=input;bad.mouseY=std::numeric_limits<float>::infinity();reject(bad);
    bad=input;bad.overloadedX=30;bad.overloadedY=-1;reject(bad);
    bad=input;bad.bannerVisible=true;bad.bannerBottom=9999;reject(bad);

    std::vector<NativeTooltipPanelLine> huge{{100,1980}};
    assert(!nativeTooltipPanelLayout(input,huge,&sentinel));
    assert(sentinel.textX==4444);
    assert(!nativeTooltipPanelInsertLine(original,4,{5,0},&withCSharp));
    assert(!nativeTooltipPanelInsertLine(original,8,{15,20},&withCSharp));
    assert(withCSharp.size()==4);

    std::cout<<"CSHARP_NATIVE_TOOLTIP_PANEL_LAYOUT=PASS variants=15\n";
}
