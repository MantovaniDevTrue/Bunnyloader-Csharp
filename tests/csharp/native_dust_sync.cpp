#include "experiments/csharp/ManagedDustMailbox.h"
#include "experiments/csharp/ManagedDustFrameBudget.h"

#include <cassert>
#include <chrono>
#include <thread>

using namespace bl::csharp;
using namespace std::chrono_literals;

static ManagedDustState state(int generation = 1) {
    ManagedDustState dust{};
    dust.slot = 320;
    dust.type = 332;
    dust.generation = generation;
    dust.active = 1;
    dust.scale = 1.f;
    dust.frameWidth = 8;
    dust.frameHeight = 8;
    return dust;
}

int main() {
    ManagedDustMailbox queue;
    const auto input = state();
    bool wake = false;
    assert(queue.offer(0, 1, input, 10, &wake, true) && wake);
    std::thread worker([&]() {
        ManagedDustMailbox::Request task{};
        assert(queue.take(&task, 1) == 1);
        assert(task.synchronous && task.phase == 1);
        auto changed = task.state;
        changed.scale = 1.5f;
        assert(queue.complete(task, changed, 1u | 2u | 16u | 32u));
    });
    ManagedDustMailbox::Response response{};
    assert(queue.await(0, 1, input, 10,
                       std::chrono::steady_clock::now() + 500ms, &response));
    assert(response.state.scale == 1.5f && response.frame == 10);
    worker.join();
    assert(!queue.consume(0, 1, 320, 332, 1, 10, &response));

    assert(queue.offer(0, 4, input, 20, &wake, true));
    assert(!queue.await(0, 4, input, 20,
                        std::chrono::steady_clock::now() + 1ms, &response));
    ManagedDustMailbox::Request late{};
    assert(queue.take(&late, 1) == 1);
    assert(late.phase == 4 && late.frame == 20 && late.synchronous);
    assert(queue.complete(late, late.state, 1u | 2u | 16u | 32u));
    assert(queue.consume(0, 4, 320, 332, 1, 21, &response));
    assert(queue.offer(0, 4, input, 21, &wake, true));
    assert(queue.take(&late, 1) == 1);
    assert(!queue.await(0, 4, input, 20,
                        std::chrono::steady_clock::now() + 1ms, &response));
    assert(queue.complete(late, late.state, 1u | 2u | 16u | 32u));
    assert(queue.consume(0, 4, 320, 332, 1, 21, &response));

    queue.resetWorld();
    assert(queue.offer(0, 3, input, 30, &wake, true));
    std::thread invalidate([&]() {
        std::this_thread::sleep_for(1ms);
        queue.resetWorld();
    });
    assert(!queue.await(0, 3, input, 30,
                        std::chrono::steady_clock::now() + 500ms, &response));
    invalidate.join();
    assert(queue.take(&late, 1) == 0);

    queue.resetWorld();
    assert(queue.offer(0, 1, input, 40, &wake, true));
    auto dying = input;
    dying.active = 0;
    assert(queue.offer(0, 5, dying, 40, &wake));
    assert(!queue.await(0, 1, input, 40,
                        std::chrono::steady_clock::now() + 2ms, &response));
    assert(queue.take(&late, 1) == 1 && late.phase == 5);

    ManagedDustFrameBudget budget;
    std::chrono::steady_clock::time_point deadline{};
    assert(budget.deadline(10, &deadline));
    assert(deadline <= std::chrono::steady_clock::now() + 250us);
    std::this_thread::sleep_for(1ms);
    budget.finish();
    assert(!budget.deadline(10, &deadline));
    assert(budget.deadline(11, &deadline));
    budget.finish();
    budget.reset();
    assert(budget.deadline(11, &deadline));
}
