#include "experiments/csharp/NativeTileSpriteReply.h"
#include <cassert>
#include <iostream>
#include <string_view>

int main() {
    using namespace bl::csharp::tile_sprite;
    bl::csharp::tile_draw::Data draw{};
    Batch batch{};
    bool hasSprites=false;
    assert(parseDrawWithSprites("DRAW|0|0|16|16|0|0|0|0|1",&draw,&batch,&hasSprites));
    assert(!hasSprites && draw.drawDefault && batch.count==0);
    assert(parseDrawWithSprites("DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,0,0,4294967295,1",
                                &draw,&batch,&hasSprites));
    assert(hasSprites&&!draw.drawDefault&&ownTileSprites(batch,754,3,4));
    assert(!ownTileSprites(batch,755,3,4));
    assert(!ownTileSprites(batch,754,4,4));
    for(std::string_view s:{
        "DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|0",
        "DRAW|0|0|16|16|0|0|0|0|0\n",
        "DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|1|753,3,4,0,0,16,16,0,0,0",
        "DRAW|0|0|16|16|0|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,0,0,0\nSPRITES|0",
        "DRAW|0|0|16|16|0|0|0|0|2\nSPRITES|1|754,3,4,0,0,16,16,0,0,0"
    })assert(!parseDrawWithSprites(s,&draw,&batch,&hasSprites));
    std::cout<<"CSHARP_TILE_SPRITE_REPLY=PASS\n";
}
