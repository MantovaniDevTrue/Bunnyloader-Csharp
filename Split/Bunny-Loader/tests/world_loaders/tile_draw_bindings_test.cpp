#include "experiments/csharp/NativeTileDrawBindings.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <string>
#include <thread>

namespace {
struct Binding {
    int generation = 0;
    int shadow = 0;
    std::string owner;
};
}

int main() {
    using bl::csharp::tile_draw::PublishedBindings;
    PublishedBindings<Binding, 64> table;
    assert(!table.load(-1));
    assert(!table.load(64));
    assert(!table.load(5));
    assert(!table.publish(-1, {1, 2, "invalid"}));
    assert(!table.publish(64, {1, 2, "invalid"}));
    assert(table.publish(5, {0, 0, "original"}));
    auto saved = table.load(5);
    assert(saved && saved->owner == "original");
    std::atomic<bool> start{false};
    std::atomic<bool> corrupt{false};
    std::thread reader([&] {
        while (!start.load(std::memory_order_acquire)) {}
        for (int i = 0; i < 8000; ++i) {
            auto value = table.load(5);
            if (!value || value->shadow != value->generation * 2 ||
                (value->generation != 0 && value->owner != std::to_string(value->generation))) {
                corrupt.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });
    start.store(true, std::memory_order_release);
    for (int i = 1; i <= 1200; ++i) {
        assert(table.publish(5, {i, i * 2, std::to_string(i)}));
    }
    reader.join();
    assert(!corrupt.load(std::memory_order_relaxed));
    assert(saved->generation == 0 && saved->owner == "original");
    assert(table.load(5)->generation == 1200);
    std::cout << "CSHARP_TILE_DRAW_NDK_COMPATIBLE_PUBLICATION=PASS\n";
}
