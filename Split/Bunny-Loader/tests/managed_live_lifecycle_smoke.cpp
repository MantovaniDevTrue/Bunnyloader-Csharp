#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    bl::csharp::ManagedLiveIngressQueue queue;
    bl::csharp::ManagedLiveQueuedEvent event{};
    std::atomic<int> primary{1137};
    std::atomic<int> wave{1136};
    std::atomic<int> secondMod{1138};
    std::atomic<bool> automatic{true};

    assert(queue.push(queue.admission(), {0, 11, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 12, true, 0, 0}, 1136, wave).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 13, false, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 14, true, 0, 1}, 1138, secondMod).accepted);
    assert(queue.push(queue.admission(), {0, 15, true, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pushAdditional(queue.admission(), {0, 16, true, 0, 0}, 1136, wave).accepted);
    assert(queue.pauseBinding(1, secondMod) == 2);
    assert(secondMod.load(std::memory_order_acquire) == -1);
    assert(queue.depth() == 4);
    assert(!queue.pushAdditional(queue.admission(), {0, 23, true, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pop(&event) && event.damage == 11 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 12 && event.bindingSlot == 0);
    assert(queue.pop(&event) && event.damage == 15 && event.bindingSlot == -1);
    assert(queue.pop(&event) && event.damage == 16 && event.bindingSlot == 0);
    assert(queue.depth() == 0);
    assert(queue.accepted() == 6);
    assert(queue.dropped() == 0);
    assert(queue.pauseBinding(1, secondMod) == 0);

    secondMod.store(1138, std::memory_order_release);
    assert(queue.pushAdditional(queue.admission(), {0, 19, false, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pop(&event) && event.damage == 19 && event.bindingSlot == 1);
    assert(queue.depth() == 0);

    queue.reset();
    secondMod.store(1138, std::memory_order_release);
    assert(queue.accepted() == 0 && queue.depth() == 0);
    assert(queue.pushAdditional(queue.admission(), {0, 21, false, 0, 1}, 1138, secondMod).accepted);
    assert(queue.pop(&event) && event.damage == 21 && event.bindingSlot == 1);
    assert(queue.push(queue.admission(), {0, 22, true, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.pop(&event) && event.damage == 22 && event.bindingSlot == -1);
    assert(queue.accepted() == 2);

    for (int cycle = 0; cycle < 32; ++cycle) {
        std::atomic<bool> start{false};
        secondMod.store(1138, std::memory_order_release);
        std::thread producer([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int i = 0; i < 12; ++i)
                queue.pushAdditional(queue.admission(), {0, 50 + cycle, true, 0, 1},
                                     1138, secondMod);
        });
        start.store(true, std::memory_order_release);
        queue.pauseBinding(1, secondMod);
        producer.join();
        assert(secondMod.load(std::memory_order_acquire) == -1);
        assert(queue.depth() == 0);
        assert(!queue.pushAdditional(queue.admission(), {0, 80, false, 0, 1},
                                     1138, secondMod).accepted);
        secondMod.store(1138, std::memory_order_release);
        assert(queue.pushAdditional(queue.admission(), {0, 81, true, 0, 1},
                                    1138, secondMod).accepted);
        assert(queue.pop(&event) && event.damage == 81 &&
               event.bindingSlot == 1);
    }
    assert(queue.depth() == 0);
    assert(queue.dropped() == 0);
    std::cout << "BUNNY_CSHARP_MOD_PAUSE_RESUME_QUEUE_BARRIER=PASS\n";
    std::cout << "BUNNY_CSHARP_MOD_PAUSE_FIFO_ISOLATION=PASS\n";
    std::cout << "BUNNY_CSHARP_WORLD_SESSION_QUEUE_RESET=PASS\n";
    return 0;
}

