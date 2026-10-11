#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include <atomic>
#include <cassert>
#include <iostream>

int main() {
    bl::csharp::ManagedLiveIngressQueue queue;
    bl::csharp::ManagedLiveQueuedEvent event{};
    std::atomic<int> primary{1137};
    std::atomic<int> wave{1136};
    std::atomic<int> secondMod{1138};
    std::atomic<bool> automatic{true};

    assert(queue.push({0, 11, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional({0, 12, true, 0, 0}, 1136, wave).accepted);
    assert(queue.pushAdditional({0, 13, false, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pushAdditional({0, 14, true, 0, 1}, 1138, secondMod).accepted);
    assert(queue.push({0, 15, true, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional({0, 16, true, 0, 0}, 1136, wave).accepted);
    secondMod.store(-1, std::memory_order_release);
    assert(queue.discardBinding(1) == 2);
    assert(queue.depth() == 4);
    assert(!queue.pushAdditional({0, 23, true, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pop(&event) && event.damage == 11 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 12 && event.bindingSlot == 0);
    assert(queue.pop(&event) && event.damage == 15 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 16 && event.bindingSlot == 0);
    assert(queue.depth() == 0);
    assert(queue.accepted() == 6);
    assert(queue.dropped() == 0);
    assert(queue.discardBinding(1) == 0);

    queue.reset();
    secondMod.store(1138, std::memory_order_release);
    assert(queue.accepted() == 0 && queue.depth() == 0);
    assert(queue.pushAdditional({0, 21, false, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pop(&event) && event.damage == 21 && event.bindingSlot == 1);
    assert(queue.push({0, 22, true, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pop(&event) && event.damage == 22 && event.bindingSlot == -1);
    assert(queue.accepted() == 2);
    std::cout << "BUNNY_CSHARP_MOD_PAUSE_FIFO_ISOLATION=PASS\n";
    std::cout << "BUNNY_CSHARP_WORLD_SESSION_QUEUE_RESET=PASS\n";
    return 0;
}
