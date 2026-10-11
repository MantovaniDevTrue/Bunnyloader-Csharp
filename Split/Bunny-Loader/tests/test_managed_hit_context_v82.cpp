#include "experiments/csharp/ManagedHitContextStats.h"

#include <cassert>
#include <cstdio>

using bl::csharp::ManagedHitContextStats;

int main() {
    ManagedHitContextStats stats;
    assert(stats.v2Calls == 0 && stats.legacyCalls == 0);
    assert(stats.lastNpcIndex == -1 && stats.uniqueNpcIndices == 0);
    stats.record(true, 137, true);
    stats.record(true, 137, true);
    stats.record(true, 0, true);
    stats.record(true, 199, false);
    stats.record(false, -1, true);
    stats.record(false, 99, false);
    assert(stats.v2Calls == 4 && stats.v2Successes == 3 && stats.v2Failures == 1);
    assert(stats.legacyCalls == 2 && stats.legacyFailures == 1);
    assert(stats.lastNpcIndex == 199 && stats.minNpcIndex == 0 && stats.maxNpcIndex == 199);
    assert(stats.uniqueNpcIndices == 3 && stats.invalidV2Indices == 0);
    stats.record(true, 200, false);
    assert(stats.v2Failures == 2 && stats.invalidV2Indices == 1);
    assert(stats.uniqueNpcIndices == 3 && stats.lastNpcIndex == 199);
    stats.record(true, -1, false);
    assert(stats.invalidV2Indices == 2 && stats.uniqueNpcIndices == 3);
    stats = {};
    assert(stats.v2Calls == 0 && stats.legacyCalls == 0 &&
           stats.minNpcIndex == -1 && stats.uniqueNpcIndices == 0);
    std::puts("CSHARP_V82_HIT_CONTEXT_COUNTERS=PASS");
}
