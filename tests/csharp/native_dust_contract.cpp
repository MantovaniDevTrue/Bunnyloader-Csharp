#include "experiments/csharp/ManagedDustContract.h"

#include <cassert>
#include <limits>

int main() {
    using namespace bl::csharp;
#if defined(BUNNY_CSHARP_DUST_PREVIEW) && BUNNY_CSHARP_DUST_PREVIEW == 1
    static_assert(managedDustCallbacksConnected());
#else
    static_assert(!managedDustCallbacksConnected());
#endif
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
    auto overflowType = sample;
    overflowType.type = 32768;
    assert(!managedDustStateValid(overflowType));
    ManagedDustState next = sample;
    next.generation++;
    assert(!managedDustSameInstance(sample, next));
    next = sample;
    next.scale = 0.f;
    assert(!managedDustStateValid(next));
    next.active = 0;
    assert(managedDustStateValid(next));
    next.active = 1;
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
    auto before = sample;
    before.x = 10.f; before.y = 5.f; before.scale = 1.f; before.alpha = 100;
    auto updated = before;
    updated.x = 11.f; updated.scale = 0.9f; updated.alpha = 90;
    auto live = before;
    live.x = 100.f; live.scale = 1.5f; live.alpha = 150;
    ManagedDustState merged{};
    assert(mergeManagedDustDelta(live, before, updated, &merged));
    assert(merged.x == 101.f);
    assert(merged.scale > 1.349f && merged.scale < 1.351f);
    assert(merged.alpha == 140);
    updated.color = 0xff123456;
    assert(mergeManagedDustDelta(live, before, updated, &merged, false));
    assert(merged.color == live.color);
    updated.active = 0;
    assert(mergeManagedDustDelta(live, before, updated, &merged));
    assert(merged.active == 0);
    updated = before;
    updated.x = std::numeric_limits<float>::quiet_NaN();
    assert(!mergeManagedDustDelta(live, before, updated, &merged));
    updated = before;
    updated.generation = 2;
    assert(!mergeManagedDustDelta(live, before, updated, &merged));
}
