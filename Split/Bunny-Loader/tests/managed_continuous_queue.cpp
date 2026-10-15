#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace bl::csharp;

int main() {
    ManagedLiveIngressQueue queue;
    queue.reset(true);
    std::atomic<int> primary{1137}, secondary{1138};
    std::atomic<bool> automatic{true};
    ManagedLiveQueuedEvent event;
    for (int i = 0; i < 1000000; ++i) {
        assert(queue.push(queue.admission(), {i, 1, false}, 1137, primary, automatic).accepted);
        assert(queue.pop(&event) && event.tick == i);
    }
    assert(queue.accepted() == 1000000 && primary == 1137);
    for (int i = 0; i < ManagedFairEventQueue::kPerBindingCapacity; ++i)
        assert(queue.push(queue.admission(), {i, 1, false}, 1137, primary, automatic).accepted);
    assert(queue.push(queue.admission(), {99, 1, false}, 1137, primary, automatic).queueDropped);
    assert(queue.bindingDrops(-1) == 1);
    assert(queue.pushAdditional(queue.admission(), {123, 1, false, 0, 0}, 1138, secondary).accepted);
    assert(queue.pop(&event) && event.bindingSlot == 0 && event.tick == 123);
    assert(queue.pop(&event) && event.bindingSlot == -1);
    const auto old = queue.admission();
    assert(queue.pauseBinding(-1, primary) == 15);
    primary = 1137;
    assert(!queue.push(old, {0, 1, false}, 1137, primary, automatic).accepted);
    queue.reset(true);
    for (int binding = -1; binding < 31; ++binding)
        for (int i = 0; i < 2; ++i) {
            const ManagedLiveQueuedEvent next{i, 1, false, 0, binding};
            assert(binding == -1 ? queue.push(queue.admission(), next, 1137, primary, automatic).accepted :
                queue.pushAdditional(queue.admission(), next, 1138, secondary).accepted);
        }
    assert(queue.depth() == 64);
    assert(queue.pushAdditional(queue.admission(), {0, 1, false, 0, 30}, 1138, secondary).queueDropped);
    for (int i = 0; i < 64; ++i) {
        assert(queue.pop(&event));
        assert(event.bindingSlot == i % 32 - 1 && event.tick == i / 32);
    }
    const auto oldWorld = queue.admission();
    queue.reset(true);
    assert(!queue.pushAdditional(oldWorld, {0, 1, false, 0, 30}, 1138, secondary).accepted);
    assert(queue.depth() == 0);
    int counter = std::numeric_limits<int>::max() - 1;
    assert(addManagedCounter(counter) == std::numeric_limits<int>::max());
    assert(addManagedCounter(counter, 100) == std::numeric_limits<int>::max());
    std::atomic<int> atomicCounter{std::numeric_limits<int>::max() - 1};
    assert(addManagedCounter(atomicCounter, 100) == std::numeric_limits<int>::max());
    std::cout << "BUNNY_CONTINUOUS_MILLION_FAIR32_BOUNDED64=PASS\n";
}
