#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bl::runtime::surface {
struct Color {uint8_t r=0,g=0,b=0,a=0;};
struct TextureSize {int width=0,height=0;};
struct BackGeometry {int loops=0;double startX=0,topY=0,step=0,scale=1;};
struct CloseSettings {double scale=1.25,parallax=.37,a=1800,b=1750;};
struct CloseGeometry {
    double screenX=0,screenY=0,worldSurface=0;
    int screenWidth=0;
    double stepTop=0;
    int scAdj=0,push=0;
};
// One compact plan per layer, rather than an allocation per repeated sprite.
struct DrawPlan {
    int loops=0,width=0,height=0;
    double startX=0,topY=0,step=0;
    float scale=1,skyDepth=0;
    bool drawSky=false;
};
bool backPlan(TextureSize texture,const BackGeometry& geometry,DrawPlan* result,std::string* error);
bool closePlan(TextureSize texture,const CloseSettings& settings,const CloseGeometry& geometry,DrawPlan* result,std::string* error);
Color multiply(Color color,double alpha);
bool modifyDefaultFades(float* fades,size_t length,size_t slot,float speed,std::string* error);
bool priorityAllows(int priority,int vanilla);
}
