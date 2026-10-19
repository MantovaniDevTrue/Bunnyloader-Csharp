#include "experiments/csharp/ManagedDustContract.h"

#include <cassert>
#include <limits>

int main() {
    using namespace bl::csharp;
    ManagedDustState sample{};
    sample.slot = 10;
    sample.type = 332;
    sample.generation = 1;
    sample.active = 1;
    sample.scale = 1.f;
    sample.frameWidth = 10;
    sample.frameHeight = 10;
    assert(managedDustStateValid(sample));
    assert(managedDustSameInstance(sample, sample));
    ManagedDustState next = sample;
    next.generation++;
    assert(!managedDustSameInstance(sample, next));
    next = sample;
    next.scale = 0.f;
    assert(!managedDustStateValid(next));
    next = sample;
    next.x = std::numeric_limits<float>::quiet_NaN();
    assert(!managedDustStateValid(next));
    next = sample;
    next.alpha = 256;
    assert(!managedDustStateValid(next));
    next = sample;
    next.frameWidth = -1;
    assert(!managedDustStateValid(next));
    next = sample;
    next.active = 2;
    assert(!managedDustStateValid(next));
    static_assert((DustOnSpawn | DustUpdate | DustMidUpdate | DustGetAlpha | DustPreDraw) == 31u);
}
