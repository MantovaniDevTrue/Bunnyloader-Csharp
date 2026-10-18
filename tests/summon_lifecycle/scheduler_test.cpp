#include "experiments/csharp/ManagedRuntime.cpp"
#include <cassert>
#include <iostream>
int main() {
 using namespace bl::csharp;
 g_pinnedStarted=true;g_entityModCount.store(1);g_entityBindings.push_back({"fixture","fixture.dll",{},{{1136,"Minion"}},0,true});
 g_entityProjectileTypeOwner[1136].store(1);g_entityNpcTypeOwner[698].store(1);
 ManagedEntityVelocity result;
 auto tick=[&](bool initial){return tickManagedEntityProjectile(1136,0,1,0,0x1234,10,20,1,0,30,40,&result,initial,-1,0,0);};
 assert(!tick(true)&&g_entityQueue.size()==1&&g_entityProjectilePending[0]);
 assert(g_entityQueue.front().identity==0x1234&&g_entityQueue.front().kind==1);
 // An in-flight callback is never submitted twice.
 for(int i=0;i<3;++i)tick(false);
 assert(g_entityQueue.size()==1);
 g_entityQueue.clear();g_entityProjectilePending[0]=false;tick(false);assert(g_entityQueue.empty());
 tick(false);assert(g_entityQueue.size()==1); // Sixth tick keeps the existing cadence.
 g_entityQueue.clear();g_entityProjectilePending[0]=false;
 g_entityProjectileResults[0]={1136,1,0,0x1234,(1LL<<32)|512|static_cast<int64_t>(256)<<16,true};
 assert(tick(false)&&result.x==2&&result.y==1);
 g_entityProjectileResults[0]={1136,999,0,0x1234,(1LL<<32)|768,true};
 assert(!tick(false)); // Stale generations cannot write to a reused slot.
 g_entityQueue.clear();g_entityNpcTicks[0]=0;
 for(int i=0;i<5;++i){tickManagedEntityNpc(698,0,1,0x4321,25,25,0,0,0,0,0,0,&result);assert(g_entityQueue.empty());}
 tickManagedEntityNpc(698,0,1,0x4321,25,25,0,0,0,0,0,0,&result);assert(g_entityQueue.size()==1&&g_entityQueue.front().kind==0);
 pauseManagedEntitySourceCallbacks();assert(g_entityQueue.empty()&&!g_entityProjectilePending[0]&&!g_entityNpcPending[0]);
 std::cout<<"PRODUCTION_MONOVM_SCHEDULER_FIRST_TICKET_SIX_TICKS_DEDUP_STALE_RESULT_AND_WORLD_PAUSE_PASS\n";
}
