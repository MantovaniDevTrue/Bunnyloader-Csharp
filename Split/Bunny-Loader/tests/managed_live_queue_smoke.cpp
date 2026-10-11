#include "experiments/csharp/ManagedLiveEventQueue.h"
#include <cassert>
#include <iostream>

using bl::csharp::ManagedLiveEventQueue;
using bl::csharp::ManagedLiveQueuedEvent;

int main() {
    static_assert(ManagedLiveEventQueue::kCapacity == 64);
    static_assert(ManagedLiveEventQueue::kEventLimit == 25000);
    ManagedLiveEventQueue queue;
    ManagedLiveQueuedEvent event{};
    assert(queue.empty());
    assert(!queue.pop(&event));
    for (int i = 1; i <= 64; ++i) {
        assert(queue.push({i, 10 + i, i % 2 == 0}));
    }
    assert(queue.size() == 64);
    assert(!queue.push({65, 75, true}));
    for (int i = 1; i <= 32; ++i) {
        assert(queue.pop(&event));
        assert(event.tick == i);
        assert(event.damage == i + 10);
        assert(event.isKill == (i % 2 == 0));
    }
    for (int i = 65; i <= 96; ++i)
        assert(queue.push({i, i + 10, i % 2 == 0}));
    for (int i = 33; i <= 96; ++i) {
        assert(queue.pop(&event));
        assert(event.tick == i);
        assert(event.damage == i + 10);
    }
    assert(queue.empty());
    queue.clear();
    for (int i = 1; i <= ManagedLiveEventQueue::kEventLimit; ++i) {
        assert(queue.push({i, 18, i % 2 == 0}));
        assert(queue.pop(&event));
        assert(event.tick == i);
    }
    for (int i = 1; i <= 64; ++i) assert(queue.push({i, 0, false}));
    queue.clear();
    assert(queue.empty());
    assert(queue.push({1, 123, true, 123456789ULL}));
    assert(queue.pop(&event) && event.tick == 1 && event.isKill);
    assert(event.enqueuedAtNs == 123456789ULL);
    std::cout << "BUNNY_MANAGED_LIVE_QUEUE_25000_FIFO64=PASS\n";
    return 0;
}
