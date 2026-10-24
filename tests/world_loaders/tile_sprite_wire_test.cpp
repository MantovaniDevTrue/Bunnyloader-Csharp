#include "experiments/csharp/NativeTileSpriteWire.h"
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

int main() {
    using namespace bl::csharp::tile_sprite;
    Batch batch{};
    assert(parse("SPRITES|0",&batch) && batch.count==0);
    assert(parse("SPRITES|1|755,12,34,0,16,16,16,2,-3,4294967295",&batch));
    assert(batch.count==1 && batch.sprites[0].type==755 &&
           batch.sprites[0].frameY==16 && batch.sprites[0].offsetX==2 &&
           batch.sprites[0].offsetY==-3 && batch.sprites[0].argb==0xffffffffu);
    assert(parse("SPRITES|2|755,12,34,0,16,16,16,2,-3,4294967295|756,0,0,18,36,16,16,0,0,2164195328",&batch));
    assert(batch.count==2 && batch.sprites[1].argb==2164195328u);
    assert(parse("SPRITES|1|755,12,34,0,16,16,16,2,-3,4294967295,1",&batch));
    assert(batch.count==1 && batch.sprites[0].effects==1);
    assert(parse("SPRITES|1|755,12,34,0,16,16,16,2,-3,4294967295,3",&batch));
    assert(batch.sprites[0].effects==3);
    assert(parse("SPRITES|1|755,12,34,0,16,16,16,2,-3,4294967295",&batch));
    assert(batch.sprites[0].effects==0 && batch.sprites[0].textureLayer==0 && batch.sprites[0].phase==0);
    assert(parse("SPRITES|1|754,3,4,0,0,16,16,0,0,4294967295,0,2,1",&batch));
    assert(batch.sprites[0].phase==1 && batch.sprites[0].textureLayer==2);
    assert(parse("SPRITES|1|754,3,4,0,0,16,16,0,0,4294967295,0,0,1",&batch));
    assert(batch.sprites[0].phase==1 && batch.sprites[0].textureLayer==0);
    const std::string sprite="755,12,34,0,0,16,16,0,0,4294967295";
    std::string eight="SPRITES|8";
    for(int i=0;i<8;++i)eight+="|"+sprite;
    assert(parse(eight,&batch) && batch.count==8);
    for(std::string_view malformed : {
        "", "SPRITES|", "SPRITES|0|", "SPRITES|-1", "SPRITES|9",
        "SPRITES|1", "SPRITES|1|", "SPRITES|1|0",
        "SPRITES|1|754,0,0,0,0,16,16,0,0,4294967295|",
        "SPRITES|1|753,12,34,0,0,16,16,0,0,4294967295",
        "SPRITES|1|65536,12,34,0,0,16,16,0,0,4294967295",
        "SPRITES|1|755,-1,34,0,0,16,16,0,0,4294967295",
        "SPRITES|1|755,12,34,32767,0,16,16,0,0,4294967295",
        "SPRITES|1|755,12,34,0,0,0,16,0,0,4294967295",
        "SPRITES|1|755,12,34,0,0,513,16,0,0,4294967295",
        "SPRITES|1|755,12,34,0,0,16,16,257,0,4294967295",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,4294967296",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,-1",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,4",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,-1",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,0,3",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,0,0,2",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,0,0,-1",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,0,0,0,1,0",
        "SPRITES|2|755,12,34,0,0,16,16,0,0,0",
        "SPRITES|1|755,12,34,0,0,16,16,0,0,x"
    }) assert(!parse(malformed,&batch));
    assert(!parse("SPRITES|0",nullptr));
    assert(!parse(std::string("SPRITES|0")+std::string(1024,'a'),&batch));
    assert(parse("SPRITES|1|754,0,0,32752,32752,16,16,-256,256,0",&batch));
    std::cout<<"CSHARP_TILE_SPRITE_WIRE=PASS\n";
}
