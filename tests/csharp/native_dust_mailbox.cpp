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
    assert(!queue.hasQueued());
    ManagedDustMailbox::Request first;
    ManagedDustMailbox::Response result;
    auto dust=sample();
    bool wake = false;
    assert(queue.offer(0,1,dust,10,&wake) && wake);
    assert(queue.hasQueued());
    assert(!queue.offer(0,1,dust,10,&wake) && !wake);
    assert(queue.take(&first,1)==1);
    assert(!queue.hasQueued());
    auto updated=dust;updated.scale=0.8f;
    assert(queue.complete(first,updated,1u | 16u | 32u));
    assert(!queue.consume(1,1,5,332,1,11,&result));
    assert(!queue.consume(0,1,5,332,1,11,&result));
    assert(queue.offer(0,1,dust,12));
    assert(queue.take(&first,1)==1);
    assert(queue.complete(first,updated,1u | 32u));
    assert(queue.consume(0,1,5,332,1,13,&result));
    assert(result.state.scale==0.8f && result.flags==(1u | 32u));
    assert(!queue.consume(0,1,5,332,1,13,&result));
    assert(queue.offer(0,1,dust,20));
    assert(queue.take(&first,1)==1);
    assert(queue.complete(first,updated,1u | 32u));
    assert(!queue.consume(0,1,5,332,1,25,&result));
    assert(queue.offer(0,1,dust,26));
    assert(queue.take(&first,1)==1);
    updated.generation=2;
    assert(!queue.complete(first,updated,1u | 32u));
    queue.resetWorld();
    assert(!queue.complete(first,dust,1u | 32u));
    assert(queue.offer(0,1,sample(5,333,2),100));
    assert(queue.take(&first,1)==1);
    assert(!queue.complete(first,dust,1u | 32u));
    assert(queue.complete(first,first.state,1u | 32u));
    assert(queue.consume(0,1,5,333,2,101,&result));
    assert(queue.offer(1,1,sample(6),102));
    assert(queue.take(&first,1)==1);
    assert(!queue.complete(first,first.state,128u));
    assert(queue.complete(first,first.state,1u | 32u));
    assert(queue.consume(1,1,6,332,1,103,&result));
    assert(queue.offer(1,1,sample(100),110,&wake) && wake);
    for(int i=1;i<128;i++)
        assert(queue.offer(1,1,sample(100+i),110,&wake) && !wake);
    assert(!queue.offer(1,1,sample(400),110));
    assert(queue.drops()>=1);
    ManagedDustMailbox::Request batch[128]{};
    assert(queue.take(batch,128)==128);
    auto phaseDust = sample(500);
    queue.resetWorld();
    assert(queue.offer(0,3,phaseDust,200));
    assert(queue.take(&first,1)==1 && first.phase==3);
    assert(queue.complete(first,first.state,1u|32u));
    assert(!queue.consume(0,4,500,332,1,201,&result));
    assert(queue.offer(0,1,phaseDust,202));
    assert(queue.take(&first,1)==1 && first.phase==1);
    assert(queue.complete(first,first.state,1u|32u));
    assert(queue.consume(0,1,500,332,1,203,&result));
    queue.resetWorld();
    assert(queue.offer(0,0,sample(2),300));
    assert(queue.offer(0,1,sample(2),300));
    ManagedDustMailbox::Request stages[2]{};
    assert(queue.take(stages,2)==2);
    assert(stages[0].phase==0 && stages[1].phase==1);
    assert(queue.complete(stages[1],stages[1].state,1u|32u));
    assert(queue.complete(stages[0],stages[0].state,1u|32u));
    assert(queue.consume(0,1,2,332,1,301,&result));
    assert(queue.consume(0,0,2,332,1,301,&result));
    assert(queue.offer(0,1,sample(2),305));
    assert(queue.take(&first,1)==1);
    assert(queue.offer(0,1,sample(2),310));
    ManagedDustMailbox::Request replacement;
    assert(queue.take(&replacement,1)==1);
    assert(!queue.complete(first,first.state,1u|32u));
    assert(queue.complete(replacement,replacement.state,1u|32u));
    assert(queue.consume(0,1,2,332,1,311,&result));
    assert(result.before.scale==result.state.scale);
    assert(queue.offer(0,1,sample(3),320));
    assert(queue.take(&first,1)==1);
    queue.discard(first);
    assert(queue.offer(0,1,sample(3),321));
    queue.resetWorld();
    auto dying = sample(770);
    dying.active = 0;
    dying.scale = 0.f;
    assert(queue.offer(0,5,dying,1));
    assert(queue.offer(0,1,sample(771),1));
    ManagedDustMailbox::Request released[2]{};
    assert(queue.take(released,2)==2);
    assert(released[0].phase==5 && released[1].phase==1);
    assert(queue.complete(released[0],dying,1u));
    assert(queue.consume(0,5,770,332,1,2,&result));
    assert(result.state.active==0 && result.state.scale==0.f);
    queue.resetWorld();
    auto aging = sample(892, 353, 4);
    assert(queue.offer(0,1,aging,61));
    assert(queue.offer(0,3,aging,61));
    ManagedDustMailbox::Request inFlight{};
    assert(queue.take(&inFlight,1)==1 && inFlight.phase==1);
    assert(queue.current(inFlight));
    auto dead = aging;
    dead.active = 0;
    dead.scale = 0;
    assert(queue.offer(0,5,dead,62));
    assert(!queue.current(inFlight));
    assert(!queue.complete(inFlight,inFlight.state,1u | 32u));
    assert(!queue.offer(0,1,aging,62));
    assert(!queue.offer(0,3,aging,63));
    ManagedDustMailbox::Request remaining[2]{};
    assert(queue.take(remaining,2)==1);
    assert(remaining[0].phase==5 && remaining[0].state.active==0);
    assert(queue.current(remaining[0]));
    queue.discard(remaining[0]);
    assert(queue.offer(0,1,sample(892,353,5),63));
    assert(queue.take(&inFlight,1)==1 && inFlight.state.generation==5);
    assert(queue.current(inFlight));
    queue.resetWorld();
    assert(!queue.current(inFlight));
    assert(!queue.complete(inFlight,inFlight.state,1u | 32u));
    assert(queue.offer(0,1,aging,1));
    queue.resetWorld();
    for (int i = 0; i < 128; ++i)
        assert(queue.offer(0,1,sample(1000+i),100));
    auto oldestSlot = sample(1127);
    assert(queue.offer(0,5,dead,101,&wake) && wake);
    assert(queue.take(&first,1)==1 && first.phase==5);
    assert(queue.current(first));
    queue.discard(first);
    assert(queue.offer(0,1,oldestSlot,102));
    assert(queue.take(batch,128)==128);
    assert(batch[127].state.slot == 1127 && batch[127].frame == 102);
    for (const auto& request:batch)
        assert(queue.current(request));
    queue.resetWorld();
    for(const auto& request:batch)
        assert(!queue.complete(request,request.state,1u | 32u));
    assert(queue.offer(0,1,sample(1200),1000));
    assert(queue.offer(0,0,sample(1201),1000));
    auto retired = sample(1202);
    retired.active = 0;
    assert(queue.offer(0,5,retired,1000));
    ManagedDustMailbox::Request ordered[3]{};
    assert(queue.take(ordered,3)==3);
    assert(ordered[0].phase==5 && ordered[1].phase==0 &&
           ordered[2].phase==1);
    queue.resetWorld();
    for(int i=0;i<128;i++)
        assert(queue.offer(0,3,sample(1300+i),1500));
    assert(queue.offer(0,0,sample(1500),1500,&wake) && wake);
    assert(queue.take(&first,1)==1 && first.phase==0);
    assert(queue.current(first));
    queue.discard(first);
    assert(queue.take(batch,128)==127);
    queue.resetWorld();
}
