#include "experiments/csharp/NativeCloudSelection.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using bl::csharp::NativeCloudCommonRefresh;
using bl::csharp::NativeCloudRenderGate;
using bl::csharp::NativeCloudSpawnAttempt;
using bl::csharp::NativeCloudWeightedPool;

int main() {
    // ExampleModBunny: a common cloud competes with 22 vanilla clouds.
    NativeCloudWeightedPool common;
    assert(common.reset(22));
    assert(common.add(300, 300, 1, true));
    assert(common.choose(0.5) == 0);
    assert(common.choose(22.5 / 23.0) == 300);
    assert(common.modCount() == 1);
    assert(!common.add(300, 300, 1, true)); // Duplicate type must not skew odds.
    assert(!common.add(301, 300, 1, false)); // No texture, no spawn.
    assert(!common.add(301, 300, NAN, true));
    assert(!common.add(301, 300, INFINITY, true));
    assert(!common.add(301, 300, -1, true));
    assert(!common.add(299, 300, 1, true));
    assert(common.choose(-0.1) == 0);
    assert(common.choose(1.0) == 0);
    assert(common.choose(NAN) == 0);
    assert(!common.reset(INFINITY));
    assert(common.choose(0.5) == 0);

    // ExampleAdvancedCloud: rare weight 10 inside example biome, 1 outside.
    NativeCloudWeightedPool rare;
    assert(rare.reset(18));
    assert(rare.add(302, 300, 10, true));
    assert(rare.choose(0.64) == 0);
    assert(rare.choose(0.65) == 302);
    assert(rare.reset(15)); // Tenth-anniversary world.
    assert(rare.add(302, 300, 1, true));
    assert(rare.choose(0.95) == 302);

    NativeCloudCommonRefresh refresh;
    assert(refresh.needsRefresh());
    refresh.sampled();
    for (int i = 0; i < 59; ++i) {
        assert(!refresh.needsRefresh());
        refresh.nextAttempt();
    }
    assert(!refresh.needsRefresh());
    refresh.nextAttempt();
    assert(refresh.needsRefresh());
    refresh.sampled();
    refresh.reset();
    assert(refresh.needsRefresh());

    // When rare RollRareCloud occurs, discard the common candidate even if
    // the rare roll chooses an ordinary vanilla cloud.
    NativeCloudSpawnAttempt attempt;
    attempt.begin(300, 7);
    auto result = attempt.result(true, 15);
    assert(result.type == 300 && result.slot == 7 && result.replaceType);
    attempt.rareRolled(302, 9);
    result = attempt.result(true, 302);
    assert(result.type == 302 && result.slot == 9 && !result.replaceType);
    assert(attempt.result(true, 1).type == 0);
    assert(attempt.result(false, 302).type == 0);
    attempt.begin(300, 3);
    attempt.rareRolled(0, -1);
    assert(attempt.result(true, 0).type == 0);
    attempt.begin(300, -1);
    assert(attempt.result(true, 0).type == 0);

    // A valid PNG with a missing horizon mask is NOT drawable on Android.
    assert(NativeCloudRenderGate::safe(300, 300, 301, 301, true, true, 64, 32, 0));
    assert(!NativeCloudRenderGate::safe(300, 300, 301, 300, true, true, 64, 32, 0));
    assert(!NativeCloudRenderGate::safe(300, 300, 301, 301, true, false, 64, 32, 0));
    assert(!NativeCloudRenderGate::safe(300, 300, 301, 301, true, true, 64, 32, 64));
    assert(!NativeCloudRenderGate::safe(300, 300, 301, 301, true, true, 0, 32, 0));

    // A malformed mod cannot force unlimited entries on the frame thread.
    assert(common.reset(22));
    for (int i = 0; i < 128; ++i) assert(common.add(300 + i, 300, 1, true));
    assert(!common.add(428, 300, 1, true));
    assert(!common.add(428, 300, std::numeric_limits<double>::max(), true));

    std::cout << "CloudLoader C#: native source-only policy OK; no APK\n";
}
