#include "experiments/csharp/ManagedDustMailbox.h"

#include <cassert>
#include <cstdint>

static bl::csharp::ManagedDustState sample(int slot=5,int type=332,int gen=1) {
    bl::csharp::ManagedDustState d{};
    d.slot=slot;d.type=type;d.generation=gen;d.active=1;
    d.scale=1.f;d.frameWidth=8;d.frameHeight=8;
    return d;
}

int main() {
    using namespace bl::csharp;
    ManagedDustMailbox queue;
    ManagedDustMailbox::Request first;
    ManagedDustMailbox::Response result;
    auto dust=sample();
    assert(queue.offer(0,dust,10));
    assert(!queue.offer(0,dust,10));
    assert(queue.take(&first,1)==1);
    auto updated=dust;updated.scale=0.8f;
    assert(queue.complete(first,updated,1u | 16u | 32u));
    assert(!queue.consume(1,5,332,1,11,&result));
    assert(!queue.consume(0,5,332,1,11,&result));
    assert(queue.offer(0,dust,12));
    assert(queue.take(&first,1)==1);
    assert(queue.complete(first,updated,1u));
    assert(queue.consume(0,5,332,1,13,&result));
    assert(result.state.scale==0.8f && result.flags==1u);
    assert(!queue.consume(0,5,332,1,13,&result));
    assert(queue.offer(0,dust,20));
    assert(queue.take(&first,1)==1);
    assert(queue.complete(first,updated,1u));
    assert(!queue.consume(0,5,332,1,25,&result));
    assert(queue.offer(0,dust,26));
    assert(queue.take(&first,1)==1);
    updated.generation=2;
    assert(!queue.complete(first,updated,1u));
    queue.resetWorld();
    assert(!queue.complete(first,dust,1u));
    assert(queue.offer(0,sample(5,333,2),100));
    assert(queue.take(&first,1)==1);
    assert(!queue.complete(first,dust,1u));
    assert(queue.complete(first,first.state,1u));
    assert(queue.consume(0,5,333,2,101,&result));
    assert(queue.offer(1,sample(6),102));
    assert(queue.take(&first,1)==1);
    assert(!queue.complete(first,first.state,128u));
    assert(queue.complete(first,first.state,1u));
    assert(queue.consume(1,6,332,1,103,&result));
    for(int i=0;i<128;i++) assert(queue.offer(1,sample(100+i),110));
    assert(!queue.offer(1,sample(400),110));
    assert(queue.drops()>=1);
    ManagedDustMailbox::Request batch[128]{};
    assert(queue.take(batch,128)==128);
    queue.resetWorld();
    for(const auto& request:batch)
        assert(!queue.complete(request,request.state,1u));
}
