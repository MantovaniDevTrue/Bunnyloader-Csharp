#include "experiments/csharp/ManagedNpcBuffEffects.h"

#include <array>
#include <atomic>
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp;
    std::array<std::atomic<uint64_t>, 32> ownerGenerations{};
    ManagedNpcBuffEffectQueue queue;
    ManagedNpcBuffCommand revoked{};
    revoked.npcIndex = 13;
    revoked.ownerBinding = 0;
    revoked.ownerGeneration = ownerGenerations[1].load();
    ManagedNpcBuffCommand independent{};
    independent.npcIndex = 14;
    independent.ownerBinding = 1;
    independent.ownerGeneration = ownerGenerations[2].load();
    ManagedNpcBuffCommand primary{};
    primary.npcIndex = 15;
    primary.ownerBinding = -1;
    primary.ownerGeneration = ownerGenerations[0].load();
    assert(queue.push(revoked));
    assert(queue.push(independent));
    assert(queue.push(primary));
    assert(queue.depth() == 3);
    assert(managedNpcBuffOwnerCurrent(revoked, ownerGenerations));
    ownerGenerations[1].fetch_add(1);
    assert(!managedNpcBuffOwnerCurrent(revoked, ownerGenerations));
    assert(managedNpcBuffOwnerCurrent(independent, ownerGenerations));
    assert(managedNpcBuffOwnerCurrent(primary, ownerGenerations));
    ManagedNpcBuffCommand next{};
    assert(queue.peek(&next) && next.npcIndex == 13);
    assert(!managedNpcBuffOwnerCurrent(next, ownerGenerations));
    queue.pop();
    assert(queue.peek(&next) && next.npcIndex == 14);
    assert(managedNpcBuffOwnerCurrent(next, ownerGenerations));
    queue.pop();
    ownerGenerations[0].fetch_add(1);
    assert(queue.peek(&next) && next.npcIndex == 15);
    assert(!managedNpcBuffOwnerCurrent(next, ownerGenerations));
    queue.pop();
    assert(queue.depth() == 0);
    ManagedNpcBuffCommand unknown{};
    unknown.ownerBinding = -2;
    assert(!managedNpcBuffOwnerCurrent(unknown, ownerGenerations));
    unknown.ownerBinding = 31;
    assert(!managedNpcBuffOwnerCurrent(unknown, ownerGenerations));
    unknown.ownerBinding = 0;
    unknown.ownerGeneration = ownerGenerations[1].load();
    assert(managedNpcBuffOwnerCurrent(unknown, ownerGenerations));
    std::cout << "MANAGED_NPC_EFFECT_OWNER_REVOCATION_OK\n";
}
