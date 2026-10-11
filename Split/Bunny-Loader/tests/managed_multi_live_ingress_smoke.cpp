#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include <atomic>
#include <cassert>
#include <iostream>

int main() {
    bl::csharp::ManagedLiveIngressQueue queue;
    bl::csharp::ManagedLiveQueuedEvent event{};
    std::atomic<int> primary{1137};
    std::atomic<int> secondary{1138};
    std::atomic<bool> automatic{true};

    assert(queue.push(queue.admission(), {0, 12, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 15, false, 0, 0}, 1138, secondary).accepted);
    assert(queue.push(queue.admission(), {0, 18, true, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 21, true, 0, 0}, 1138, secondary).accepted);
    assert(queue.pop(&event) && event.damage == 12 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 15 && event.bindingSlot == 0);
    assert(queue.pop(&event) && event.damage == 18 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 21 && event.bindingSlot == 0);

    assert(!queue.pushAdditional(queue.admission(), {0, 7, false, 0, 0}, 9999, secondary).accepted);
    assert(!queue.push(queue.admission(), {0, 7, false, 0, -1}, 9999, primary, automatic).accepted);
    secondary.store(-1);
    assert(!queue.pushAdditional(queue.admission(), {0, 15, true, 0, 0}, 1138, secondary).accepted);
    secondary.store(1138);

    for (int i = 4; i < 25000; ++i) {
        const bool extra = (i % 2) == 0;
        const bl::csharp::ManagedLiveQueuedEvent next{
            0, i, extra, 0, extra ? 0 : -1
        };
        const bool accepted = extra ?
            queue.pushAdditional(queue.admission(), next, 1138, secondary).accepted :
            queue.push(queue.admission(), next, 1137, primary, automatic).accepted;
        assert(accepted && queue.pop(&event));
        assert(event.damage == i && event.bindingSlot == (extra ? 0 : -1));
    }
    assert(queue.accepted() == 25000);
    assert(!queue.pushAdditional(queue.admission(), {0, 15, false, 0, 0}, 1138, secondary).accepted);
    assert(!queue.push(queue.admission(), {0, 15, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.dropped() == 0);
    queue.reset();
    assert(queue.accepted() == 0);
    assert(queue.pushAdditional(queue.admission(), {0, 1, true, 0, 0}, 1138, secondary).accepted);
    assert(queue.pop(&event) && event.bindingSlot == 0);
    std::cout << "BUNNY_CSHARP_MULTI_PROJECTILE_FIFO64_25000=PASS\n";
}

