#include "experiments/csharp/ManagedDustLifecycle.h"

#include <cassert>

using namespace bl::csharp;

ManagedDustState make(int slot, int type = 332, int generation = 1) {
    ManagedDustState state{};
    state.slot = slot;
    state.type = type;
    state.generation = generation;
    state.active = 1;
    state.scale = 1.f;
    state.frameWidth = 8;
    state.frameHeight = 8;
    return state;
}

int main() {
    ManagedDustLifecycle life;
    life.observe(make(5));
    life.observe(make(5));
    assert(life.trackedCount() == 1 && life.pendingCount() == 0);
    life.observe(make(5, 332, 2));
    assert(life.pendingCount() == 1);
    ManagedDustState released{};
    assert(life.flush(1, [&](const ManagedDustState& d) {
        released = d;
        return true;
    }) == 1);
    assert(released.slot == 5 && released.generation == 1 &&
           released.active == 0 && life.pendingCount() == 0);
    life.observe(make(9, 333, 4));
    life.sweep([](int slot, int, int) { return slot == 5; });
    assert(life.trackedCount() == 1 && life.pendingCount() == 1);
    assert(life.flush(1, [](const ManagedDustState&) { return false; }) == 0);
    assert(life.pendingCount() == 1);
    assert(life.flush(1, [&](const ManagedDustState& d) {
        assert(d.slot == 9 && d.generation == 4 && d.active == 0);
        return true;
    }) == 1);
    life.retire(5);
    assert(life.pendingCount() == 1);
    life.sweep([](int, int, int) { return true; });
    assert(life.trackedCount() == 0);
    life.observe(make(21, 334, 3));
    life.observe(make(22, 334, 3));
    life.sweep([](int slot, int type, int gen) {
        return slot == 21 && type == 334 && gen == 3;
    });
    assert(life.trackedCount() == 1 && life.pendingCount() == 2);
    size_t accepted = life.flush(4, [](const ManagedDustState&) { return true; });
    assert(accepted == 2 && life.pendingCount() == 0);
    life.reset();
    assert(life.trackedCount() == 0 && life.pendingCount() == 0);
    life.observe(make(2, 332, 1));
    life.observe(make(2, 333, 1));
    assert(life.pendingCount() == 1);
    assert(life.flush(1, [&](const ManagedDustState& d) {
        return d.type == 332;
    }) == 1);
    life.reset();
    assert(life.pendingCount() == 0);
}
