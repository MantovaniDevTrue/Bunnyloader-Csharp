#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedHitCallbackDispatch.h"
#include "experiments/csharp/ManagedHitContextStats.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>
using namespace bl::csharp;
int main() {
    static_assert(std::is_trivially_copyable_v<ManagedLiveQueuedEvent>);
    static_assert(std::is_trivially_copyable_v<ManagedPlayerSnapshot>);
    const ManagedNpcSnapshot npc{320, 180, 400.5f, 250.25f, true};
    const ManagedPlayerSnapshot player{0, 400, 500, 360.5f, 230.75f, true};
    assert(managedPlayerSnapshotIsValid(player));
    assert(!managedPlayerSnapshotIsValid({-1,400,500,1,1,true}));
    assert(!managedPlayerSnapshotIsValid({0,-2,500,1,1,true}));
    assert(!managedPlayerSnapshotIsValid({0,400,200,1,1,true}));
    assert(!managedPlayerSnapshotIsValid({0,400,500,std::numeric_limits<float>::infinity(),1,true}));
    int legacy=0,v2=0,v3=0,v4=0;
    assert(chooseManagedHitCallback(&legacy,&v2,26,&v3,true,&v4,true).method==&v4);
    assert(chooseManagedHitCallback(&legacy,&v2,26,&v3,true,&v4,false).method==&v3);
    assert(chooseManagedHitCallback(&legacy,&v2,26,&v3,false,&v4,true).method==&v2);
    assert(chooseManagedHitCallback(&legacy,&v2,-1,&v3,true,&v4,true).method==&legacy);
    ManagedLiveEventQueue queue;
    assert(queue.push({0,12,false,0,-1,26,npc,player}));
    ManagedLiveQueuedEvent event;
    assert(queue.pop(&event));
    assert(event.player.life==400 && event.npc.npcType==320);
    ManagedHitContextStats stats;
    stats.recordV4(26,event.npc,event.player,true);
    assert(stats.v4Calls==1 && stats.v4Successes==1);
    assert(stats.v3Calls==0 && stats.lastPlayerLife==400);
    assert(stats.lastNpcIndex==26 && stats.uniqueNpcIndices==1);
    std::puts("CSHARP_V84_PLAYER_NPC_SNAPSHOT_FIFO=PASS");
}
