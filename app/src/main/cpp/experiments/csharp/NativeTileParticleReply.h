#pragma once
#include "experiments/csharp/NativeTileSpriteReply.h"
#include "experiments/csharp/NativeTileParticleWire.h"

namespace bl::csharp::tile_particle {

inline bool parseReply(std::string_view text,tile_draw::Data* draw,
                       tile_sprite::Batch* sprites,bool* hasSprites,
                       Batch* particles,bool* hasParticles) {
    if(!draw||!sprites||!hasSprites||!particles||!hasParticles||
       text.size()>1024)return false;
    const size_t split=text.find("\nPARTICLES|");
    const auto prior=split==std::string_view::npos?text:text.substr(0,split);
    if(!tile_sprite::parseDrawWithSprites(prior,draw,sprites,hasSprites))return false;
    *particles=Batch{};
    *hasParticles=false;
    if(split==std::string_view::npos)return true;
    if(text.find('\n',split+1)!=std::string_view::npos ||
       !parse(text.substr(split+1),particles))return false;
    *hasParticles=true;
    return true;
}
}
