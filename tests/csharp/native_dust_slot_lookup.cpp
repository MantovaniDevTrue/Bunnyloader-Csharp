#include "experiments/csharp/ManagedDustSlotLookup.h"

#include <cassert>

using bl::csharp::ManagedDustSlotLookup;

int main() {
    ManagedDustSlotLookup<int, 6> resolver;
    int value[8]{};
    int* slots[7] = {&value[0], &value[1], &value[2], &value[3],
                     &value[4], &value[5], nullptr};
    assert(resolver.resolve(slots, 5, &value[0]) == -1);
    assert(resolver.resolve(nullptr, 7, &value[0]) == -1);
    assert(resolver.resolve(slots, 7, nullptr) == -1);
    assert(resolver.resolve(slots, 7, &value[4], 4) == 4);
    assert(resolver.knownCount() == 0);
    assert(resolver.resolve(slots, 7, &value[4], 0) == 4);
    assert(resolver.knownCount() == 6);
    assert(resolver.resolve(slots, 7, &value[4], 500) == 4);
    assert(resolver.resolve(slots, 7, &value[6]) == -1);
    slots[4] = &value[6];
    assert(resolver.resolve(slots, 7, &value[4]) == -1);
    assert(resolver.resolve(slots, 7, &value[6]) == 4);
    int* newWorld[6] = {&value[6], &value[4], &value[3],
                        &value[2], &value[1], &value[0]};
    assert(resolver.resolve(newWorld, 6, &value[4], 4) == 1);
    resolver.reset();
    assert(resolver.knownCount() == 0);
    assert(resolver.resolve(newWorld, 6, &value[6]) == 0);
    resolver.reset();
    resolver.remember(slots, 7, 4, &value[6]);
    assert(resolver.knownCount() == 1);
    assert(resolver.resolve(slots, 7, &value[6], -1) == 4);
    int newDust = 9;
    slots[4] = &newDust;
    resolver.remember(slots, 7, 4, &newDust);
    assert(resolver.knownCount() == 1);
    assert(resolver.resolve(slots, 7, &newDust) == 4);
    assert(resolver.resolve(slots, 7, &value[6]) == -1);
}
