#include "experiments/csharp/NativeTileOverlayPolicy.h"
#include "experiments/csharp/NativeTileSpriteWire.h"
#include <cassert>
#include <iostream>
#include <string_view>

int main() {
    using namespace bl::csharp;
    assert(tile_overlay::path("/mods/a/Content/Tiles/Campfire.png",2)==
           "/mods/a/Content/Tiles/Campfire_Flame.png");
    assert(tile_overlay::path("/mods/a/Content/Tiles/Monolith.png",1)==
           "/mods/a/Content/Tiles/Monolith_Glow.png");
    assert(tile_overlay::path("/mods/a/Content/Tiles/Monolith.png",3).empty());
    assert(tile_overlay::path("/mods/a/Content/Tiles/Monolith.jpg",1).empty());
    assert(tile_overlay::frameInside(0,36,16,16,54,72));
    assert(!tile_overlay::frameInside(0,60,16,16,54,72));
    assert(!tile_overlay::frameInside(0,0,0,16,54,72));
    tile_sprite::Batch batch{};
    assert(tile_sprite::parse("SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0,1",&batch));
    assert(batch.sprites[0].textureLayer==1);
    assert(tile_sprite::parse("SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0,2",&batch));
    assert(batch.sprites[0].textureLayer==2);
    assert(tile_sprite::parse("SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0",&batch));
    assert(batch.sprites[0].textureLayer==0);
    for(std::string_view value:{
        "SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0,3",
        "SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0,-1",
        "SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,0,1,2",
        "SPRITES|1|754,3,4,0,36,16,16,0,0,4294967295,,1"
    })assert(!tile_sprite::parse(value,&batch));
    std::cout<<"CSHARP_TILE_OVERLAYS=PASS\n";
}
