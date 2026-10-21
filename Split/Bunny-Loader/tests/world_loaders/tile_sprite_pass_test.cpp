#include "experiments/csharp/NativeTileSpritePassQueue.h"
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    using namespace bl::csharp::tile_sprite;
    assert(eligibleDrawPass(false,false,false));
    assert(eligibleDrawPass(false,false,false));
    assert(!eligibleDrawPass(true,false,false));
    assert(!eligibleDrawPass(false,true,false));
    assert(!eligibleDrawPass(false,false,true));
    SpritePassQueue pending;
    Batch batch{};
    batch.count=1;
    batch.sprites[0].type=754;
    batch.sprites[0].x=8;
    batch.sprites[0].y=9;
    assert(!pending.stage(0,batch));
    assert(pending.stage(1,batch));
    assert(pending.pending());
    assert(pending.pendingFor(1));
    assert(!pending.pendingFor(2));
    std::array<Sprite,kMaxStagedSprites> result{};
    size_t length=0;
    assert(!pending.take(2,&result,&length));
    assert(pending.pendingFor(1));
    assert(pending.take(1,&result,&length));
    assert(length==1 && result[0].x==8 && !pending.pending());
    assert(!pending.take(1,&result,&length));
    for(int i=0;i<32;++i) assert(pending.stage(2,batch));
    assert(!pending.stage(2,batch));
    assert(pending.take(2,&result,&length)&&length==32);
    Batch invalid=batch;
    invalid.sprites[0].type=753;
    assert(!pending.stage(3,invalid));
    invalid=batch;invalid.count=9;
    assert(!pending.stage(3,invalid));
    assert(!pending.pending());
    std::thread producer([&] {for(int i=0;i<32;++i)assert(pending.stage(4,batch));});
    producer.join();
    assert(pending.take(4,&result,&length)&&length==32);
    assert(pending.stage(5,batch));
    assert(!pending.stage(4,batch));
    assert(!pending.take(4,&result,&length));
    assert(pending.stage(6,batch));
    assert(!pending.pendingFor(5));
    assert(pending.pendingFor(6));
    assert(pending.take(6,&result,&length)&&length==1);
    pending.clear();
    assert(!pending.pending());
    std::cout<<"CSHARP_TILE_SPRITE_PASS_QUEUE=PASS\n";
}
