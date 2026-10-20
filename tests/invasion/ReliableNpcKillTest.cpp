#include "../../app/src/main/cpp/experiments/csharp/ReliableNpcKill.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace bl::csharp {
static bool workerAvailable = false;
static std::vector<uintptr_t> dispatched;

bool queueManagedEntityCombatEvent(int kind, int type, int slot, int generation,
                                   int owner, uintptr_t identity, int damage, int target) {
    if (kind != 3 || type != 700 || slot < 0 || generation <= 0 ||
        owner != -1 || damage != 0 || target != -1 || !workerAvailable)
        return false;
    dispatched.push_back(identity);
    return true;
}
}

int main() {
    using namespace bl::csharp;
    resetReliableNpcKills();
    workerAvailable = false;
    assert(queueReliableNpcKill(700, 1, 1, 1001));
    assert(queueReliableNpcKill(700, 1, 1, 1001));
    assert(queueReliableNpcKill(700, 2, 1, 1002));
    assert(pendingReliableNpcKills() == 2);
    pumpReliableNpcKillsOnGameThread();
    assert(pendingReliableNpcKills() == 2);
    workerAvailable = true;
    pumpReliableNpcKillsOnGameThread();
    assert(pendingReliableNpcKills() == 0);
    assert(dispatched.size() == 2 &&
           dispatched[0] == 1001 && dispatched[1] == 1002);

    workerAvailable = false;
    assert(queueReliableNpcKill(700, 3, 1, 1003));
    resetReliableNpcKills();
    workerAvailable = true;
    pumpReliableNpcKillsOnGameThread();
    assert(dispatched.size() == 2);
    assert(!queueReliableNpcKill(700, 512, 1, 1234));
    assert(!queueReliableNpcKill(700, 0, 0, 1234));
    assert(!queueReliableNpcKill(700, 0, 1, 0));

    workerAvailable = false;
    for (int i = 0; i < 512; i++)
        assert(queueReliableNpcKill(700, i, 2, static_cast<uintptr_t>(2000 + i)));
    assert(pendingReliableNpcKills() == 512);
    assert(!queueReliableNpcKill(700, 0, 3, 9900));
    assert(droppedReliableNpcKills() >= 1);
    workerAvailable = true;
    for (int i = 0; i < 32; i++) pumpReliableNpcKillsOnGameThread();
    assert(pendingReliableNpcKills() == 0);
    assert(dispatched.size() == 514);
    return 0;
}
