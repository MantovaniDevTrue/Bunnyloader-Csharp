#pragma once
#include "experiments/csharp/NativeTileDrawWire.h"
#include "experiments/csharp/NativeTileSpriteWire.h"
#include <string_view>

namespace bl::csharp::tile_sprite {

inline bool parseDrawWithSprites(std::string_view payload, tile_draw::Data* draw,
                                 Batch* batch, bool* hasSprites) {
    if(!draw||!batch||!hasSprites||payload.size()>1024)return false;
    const auto sep=payload.find('\n');
    if(sep==std::string_view::npos) {
        if(!tile_draw::parse(payload,draw))return false;
        *batch=Batch{};
        *hasSprites=false;
        return true;
    }
    if(payload.find('\n',sep+1)!=std::string_view::npos||
       !tile_draw::parse(payload.substr(0,sep),draw)||
       !parse(payload.substr(sep+1),batch)||batch->count==0)
        return false;
    *hasSprites=true;
    return true;
}

inline bool ownTileSprites(const Batch& batch,int type,int x,int y) {
    if(batch.count==0||batch.count>kMaxSprites)return false;
    for(size_t i=0;i<batch.count;i++)
        if(!valid(batch.sprites[i])||batch.sprites[i].type!=type||
           batch.sprites[i].x!=x||batch.sprites[i].y!=y)return false;
    return true;
}

}
