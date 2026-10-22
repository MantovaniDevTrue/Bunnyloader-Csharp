#include "content/backgrounds/SurfaceGeometry.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bl::runtime::surface {
namespace {
bool fail(std::string* error,const char* stage,const char* reason,const std::string& actual) {
    if(error)*error="[Native][ERROR][SurfaceBackground] Stage="+std::string(stage)+" Expected=FiniteBoundedLayerGeometry Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}
bool scalar(double value){return std::isfinite(value) && std::abs(value)<=std::numeric_limits<float>::max();}
bool textureValid(TextureSize t){return t.width>0 && t.height>0 && t.width<=32768 && t.height<=32768;}
}
bool backPlan(TextureSize texture,const BackGeometry& g,DrawPlan* result,std::string* error) {
    if(!result || !textureValid(texture) || g.loops<0 || g.loops>32770 || !scalar(g.startX) || !scalar(g.topY) ||
       !scalar(g.step) || !scalar(g.scale) || float(g.scale)<=0 || !scalar(g.startX+g.step*std::max(0,g.loops-1)))
        return fail(error,"PlanBackLayer","InvalidBackGeometry","Loops:"+std::to_string(g.loops)+",Scale:"+std::to_string(g.scale));
    *result={g.loops,texture.width,texture.height,g.startX,g.topY,g.step,float(g.scale),0,false};return true;
}
bool closePlan(TextureSize texture,const CloseSettings& s,const CloseGeometry& g,DrawPlan* result,std::string* error) {
    if(!result || !textureValid(texture) || g.screenWidth<=0 || g.screenWidth>32768 || !scalar(s.scale*2) || s.scale<=0 ||
       !std::isfinite(s.parallax) || s.parallax<=0 || !scalar(1/s.parallax) || float(1/s.parallax)<=0 || !std::isfinite(s.a) || !std::isfinite(s.b) ||
       !std::isfinite(g.screenX) || !std::isfinite(g.screenY) || !std::isfinite(g.worldSurface) || !std::isfinite(g.stepTop) || !std::isfinite(g.scAdj))
        return fail(error,"PlanCloseLayer","InvalidCloseGeometry","ScreenWidth:"+std::to_string(g.screenWidth)+",Scale:"+std::to_string(s.scale)+",Parallax:"+std::to_string(s.parallax));
    const double scale=s.scale*2,width=std::floor(texture.width*scale),x=g.screenX*s.parallax;
    if(!(width>0) || !scalar(width))return fail(error,"PlanCloseLayer","InvalidCloseProjection","Width:"+std::to_string(width));
    // JavaScript Math.round ties toward +infinity. std::round differs at
    // negative half-widths and produces a visible one-sprite displacement.
    const double start=std::floor(-(x-width*std::floor(x/width+.5))-width/2);
    const double top=std::floor(g.stepTop*s.a+s.b)+g.scAdj+double(g.push);
    const double loops=std::floor(g.screenWidth/width)+2;
    if(!(width>0) || !scalar(width) || !scalar(start) || !scalar(top) || loops<0 || loops>32770 ||
       !scalar(start+width*std::max(0.0,loops-1)) || !std::isfinite(g.worldSurface*16+16))
        return fail(error,"PlanCloseLayer","InvalidCloseProjection","Width:"+std::to_string(width)+",Loops:"+std::to_string(loops));
    // DrawToDepth precedes the underground clip in the original loader.
    *result={g.screenY>=g.worldSurface*16+16?0:int(loops),texture.width,texture.height,
             start,top,width,float(scale),float(1/s.parallax),true};return true;
}
Color multiply(Color color,double alpha) {
    if(!std::isfinite(alpha))return {};
    auto channel=[&](uint8_t value){return static_cast<uint8_t>(std::clamp(double(value)*alpha,0.0,255.0));};
    return {channel(color.r),channel(color.g),channel(color.b),channel(color.a)};
}
bool modifyDefaultFades(float* fades,size_t length,size_t slot,double speed,std::string* error) {
    if(!fades || !length || length>16384 || slot>=length || !std::isfinite(speed))
        return fail(error,"ModifyFarFades","InvalidFadeContract","Length:"+std::to_string(length)+",Slot:"+std::to_string(slot));
    // Inspect every input before writing; one invalid element cannot leave a
    // partially changed fade array for a different style or language.
    const auto next=[&](size_t i){return i==slot?std::min(1.0,double(fades[i])+speed):std::max(0.0,double(fades[i])-speed);};
    for(size_t i=0;i<length;++i)if(!std::isfinite(fades[i]) || !scalar(next(i)))
        return fail(error,"ModifyFarFades","NonFiniteFade","Index:"+std::to_string(i));
    // Match the original JS Number calculation, then store Single once. A
    // Double native transition speed must not be rounded before the addition.
    for(size_t i=0;i<length;++i)fades[i]=float(next(i));
    return true;
}
bool priorityAllows(int priority,int vanilla) {
    const bool forest=vanilla==0 || vanilla==10 || vanilla==11 || vanilla==12;
    return priority>=3 || (priority>=2 && (forest || vanilla==3 || vanilla==7)) || (priority>=1 && forest);
}
}
