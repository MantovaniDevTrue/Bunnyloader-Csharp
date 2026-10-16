#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedHitCallbackDispatch.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>

int main() {
    using namespace bl::csharp;
    static_assert(std::is_trivially_copyable_v<ManagedNpcSnapshot>);
    static_assert(std::is_trivially_copyable_v<ManagedLiveQueuedEvent>);
    const ManagedNpcSnapshot valid{697, 150, 112.5f, 80.25f, true};
    assert(managedNpcSnapshotIsValid(valid));
    assert(!managedNpcSnapshotIsValid({697, 150, 0, 0, false}));
    assert(!managedNpcSnapshotIsValid({-1, 150, 0, 0, true}));
    assert(!managedNpcSnapshotIsValid({697, 0, 0, 0, true}));
    assert(!managedNpcSnapshotIsValid({697, 150, std::numeric_limits<float>::infinity(), 0, true}));
    assert(!managedNpcSnapshotIsValid({697, 150, 0, std::numeric_limits<float>::quiet_NaN(), true}));
    int legacy = 0, v2 = 0, v3 = 0;
    const auto choose = [&](int index, bool validContext) {
        return chooseManagedHitCallback(&legacy, &v2, index, &v3, validContext);
    };
    assert(choose(43, true).method == &v3 && choose(43, true).usesV3);
    assert(choose(43, false).method == &v2 && choose(43, false).usesV2);
    assert(choose(-1, true).method == &legacy);
    assert(choose(200, true).method == &legacy);
    ManagedLiveEventQueue events;
    assert(events.push({0, 17, false, 0, -1, 43, valid}));
    ManagedLiveQueuedEvent received;
    assert(events.pop(&received));
    assert(received.npcIndex == 43 && received.npc.npcType == 697);
    assert(received.npc.posX == 112.5f && received.npc.posY == 80.25f);
    std::puts("CSHARP_V83_EVENT_SNAPSHOT_FIFO_FALLBACK=PASS");
}
