#pragma once
#include <string>
#include <string_view>

namespace bl::csharp::tile_overlay {

inline std::string path(std::string_view tileTexture,int variant) {
    if(variant!=1&&variant!=2)return {};
    if(tileTexture.size()<=4||tileTexture.substr(tileTexture.size()-4)!=".png")
        return {};
    std::string result(tileTexture.substr(0,tileTexture.size()-4));
    result+=variant==1?"_Glow.png":"_Flame.png";
    return result;
}

inline constexpr bool frameInside(int frameX,int frameY,int width,int height,
                                  int textureWidth,int textureHeight) {
    return frameX>=0&&frameY>=0&&width>0&&height>0 &&
           textureWidth>=width&&textureHeight>=height &&
           frameX<=textureWidth-width&&frameY<=textureHeight-height;
}

}
