#include "experiments/csharp/ManagedLiveIngressQueue.h"

#include <array>
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

int main() {
    using bl::csharp::ManagedLiveIngressQueue;
    using bl::csharp::ManagedLiveQueuedEvent;

    ManagedLiveIngressQueue ingress;
    std::atomic<int> type{1136};
    std::atomic<bool> automatic{true};
    std::atomic<bool> finished{false};
    int received = 0;
    std::thread consumer([&] {
        ManagedLiveQueuedEvent event;
        int previous = 0;
        while (!finished.load(std::memory_order_acquire) || ingress.depth() != 0) {
            if (!ingress.pop(&event)) {
                std::this_thread::yield();
                continue;
            }
            assert(event.tick == previous + 1);
            previous = event.tick;
            ++received;
        }
    });

    for (int tick = 1; tick <= 25000; ) {
        const auto result = ingress.push(ingress.admission(),
            {tick, 18, (tick & 1) == 0}, 1136, type, automatic);
        if (result.accepted) ++tick;
        else assert(result.queueDropped);
    }
    finished.store(true, std::memory_order_release);
    consumer.join();
    assert(received == 25000);
    assert(ingress.accepted() == 25000);
    assert(ingress.depth() == 0);
    assert(ingress.maxDepth() <= 64);
    assert(type.load(std::memory_order_acquire) == -1);
    assert(!ingress.push(ingress.admission(), {25001, 18, false}, 1136, type, automatic).accepted);

    ingress.reset();
    type.store(1136, std::memory_order_release);
    automatic.store(false, std::memory_order_release);
    assert(!ingress.push(ingress.admission(), {1, 18, true}, 1136, type, automatic).accepted);
    assert(ingress.push(ingress.admission(), {1, 18, false}, 1136, type, automatic).accepted);
    assert(ingress.clear() == 1);
    assert(ingress.depth() == 0);

    ingress.reset();
    automatic.store(true, std::memory_order_release);
    constexpr int kThreads = 4;
    constexpr int kPerThread = 5000;
    std::array<int, kThreads * kPerThread> seen{};
    std::atomic<bool> producersDone{false};
    int drained = 0;
    std::thread drain([&] {
        ManagedLiveQueuedEvent event;
        while (!producersDone.load(std::memory_order_acquire) || ingress.depth() != 0) {
            if (!ingress.pop(&event)) {
                std::this_thread::yield();
                continue;
            }
            assert(event.damage >= 0 && event.damage < static_cast<int>(seen.size()));
            assert(seen[static_cast<size_t>(event.damage)] == 0);
            ++seen[static_cast<size_t>(event.damage)];
            ++drained;
        }
    });
    std::vector<std::thread> producers;
    for (int producer = 0; producer < kThreads; ++producer) {
        producers.emplace_back([&, producer] {
            for (int i = 0; i < kPerThread; ++i) {
                const auto result = ingress.push(ingress.admission(),
                    {0, producer * kPerThread + i, false}, 1136, type, automatic);
                assert(result.accepted || result.queueDropped);
            }
        });
    }
    for (auto& producer : producers) producer.join();
    producersDone.store(true, std::memory_order_release);
    drain.join();
    assert(ingress.accepted() + ingress.dropped() == kThreads * kPerThread);
    assert(drained == ingress.accepted());
    assert(ingress.depth() == 0);
}

