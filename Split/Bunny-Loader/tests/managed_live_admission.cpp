#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
using namespace bl::csharp;
int main() {
    ManagedLiveIngressQueue queue;
    std::atomic<int> primary{1137}, secondary{1138};
    std::atomic<bool> automatic{true};
    std::atomic<bool> captured{false}, resume{false};
    std::thread producer([&] {
        auto old = queue.admission();
        assert(secondary.load() == 1138);
        captured.store(true, std::memory_order_release);
        while (!resume.load(std::memory_order_acquire)) std::this_thread::yield();
        assert(!queue.pushAdditional(old, {0, 1, false, 0, 0}, 1138, secondary).accepted);
        assert(queue.push(old, {0, 2, false, 0, -1}, 1137, primary, automatic).accepted);
    });
    while (!captured.load(std::memory_order_acquire)) std::this_thread::yield();
    queue.pauseBinding(0, secondary);
    secondary.store(1138, std::memory_order_release);
    resume.store(true, std::memory_order_release);
    producer.join();
    assert(queue.pushAdditional(queue.admission(), {0, 3, true, 0, 0}, 1138, secondary).accepted);
    auto previousWorld = queue.admission();
    queue.reset();
    assert(!queue.push(previousWorld, {0, 4, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(!queue.pushAdditional(previousWorld, {0, 5, false, 0, 0}, 1138, secondary).accepted);
    assert(queue.push(queue.admission(), {0, 6, false, 0, -1}, 1137, primary, automatic).accepted);
    auto beforeClear = queue.admission();
    assert(queue.clear() == 1);
    assert(!queue.push(beforeClear, {0, 7, false, 0, -1}, 1137, primary, automatic).accepted);
    assert(queue.depth() == 0);
    assert(!queue.pushAdditional(queue.admission(), {0, 8, false, 0, 99}, 1138, secondary).accepted);
    std::cout << "MANAGED_STALE_PRODUCER_PAUSE_AND_WORLD_BARRIER=PASS\n";
}
