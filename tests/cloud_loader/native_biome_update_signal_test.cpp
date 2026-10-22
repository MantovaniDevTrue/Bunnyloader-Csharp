#include "experiments/csharp/NativeBiomeUpdateSignal.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    bl::csharp::NativeBiomeUpdateSignal signal;
    signal.noteLocalPlayerUpdate();
    assert(signal.takePending() == 0);
    assert(!signal.armed());

    signal.arm();
    assert(signal.armed());
    signal.noteLocalPlayerUpdate();
    signal.noteLocalPlayerUpdate();
    assert(signal.takePending() == 2);
    assert(signal.takePending() == 0);

    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i)
        producers.emplace_back([&] {
            for (int j = 0; j < 1000; ++j)
                signal.noteLocalPlayerUpdate();
        });
    for (auto& worker : producers) worker.join();
    assert(signal.takePending() == 64);
    assert(signal.takeOverflow());
    assert(!signal.takeOverflow());
    signal.noteLocalPlayerUpdate();
    assert(signal.takePending() == 1);

    signal.pause();
    signal.noteLocalPlayerUpdate();
    assert(!signal.armed() && signal.takePending() == 0);
    signal.arm();
    assert(signal.takePending() == 0);
    signal.noteLocalPlayerUpdate();
    signal.pause();
    signal.arm();
    assert(signal.takePending() == 0);

    std::cout << "BunnyBiome post-UpdateBiomes signal is bounded and reset-safe OK\n";
}
