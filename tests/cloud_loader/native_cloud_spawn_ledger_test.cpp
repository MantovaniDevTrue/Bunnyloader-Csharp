#include "experiments/csharp/NativeCloudSpawnLedger.h"

#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using bl::csharp::NativeCloudSpawnLedger;
    NativeCloudSpawnLedger ledger;
    NativeCloudSpawnLedger::Event old{}, current{}, taken{};
    assert(ledger.push(50, 8, 0.85f, 1, &old));
    assert(ledger.pending() == 1);
    assert(ledger.stillCurrent(old, true, 50, 0.85f, 0));
    assert(!ledger.stillCurrent(old, false, 50, 0.85f, 0));
    assert(!ledger.stillCurrent(old, true, 51, 0.85f, 0));
    assert(!ledger.stillCurrent(old, true, 50, 0.9f, 0));
    assert(!ledger.stillCurrent(old, true, 50, 0.85f, 4));

    assert(ledger.push(51, 8, 0.75f, 0, &current));
    assert(current.generation != old.generation);
    assert(!ledger.stillCurrent(old, true, 50, 0.85f, 0));
    assert(ledger.stillCurrent(current, true, 51, 0.75f, 3));
    assert(ledger.take(&taken) && taken.type == 50);
    assert(ledger.take(&taken) && taken.type == 51);
    assert(!ledger.take(&taken));

    ledger.invalidate(8);
    assert(!ledger.stillCurrent(current, true, 51, 0.75f, 3));

    assert(!ledger.push(60, 200, 1, 0));
    assert(!ledger.push(60, -1, 1, 0));
    assert(!ledger.push(60, 0, 0, 0));
    assert(!ledger.push(60, 0, std::numeric_limits<float>::quiet_NaN(), 0));
    assert(!ledger.push(60, 0, 1, 8));
    assert(!ledger.push(-1, 0, 1, 0));

    ledger.clear();
    for (int n = 0; n < static_cast<int>(NativeCloudSpawnLedger::kMaxPending); ++n)
        assert(ledger.push(60, n % 200, 1.0f, 0));
    assert(ledger.pending() == NativeCloudSpawnLedger::kMaxPending);
    assert(!ledger.push(60, 199, 1.f, 0));
    assert(ledger.pending() == NativeCloudSpawnLedger::kMaxPending);
    for (int n = 0; n < static_cast<int>(NativeCloudSpawnLedger::kMaxPending); ++n)
        assert(ledger.take(&taken));
    assert(!ledger.take(&taken));

    assert(ledger.push(60, 199, 0.7f, 1, &current));
    assert(ledger.stillCurrent(current, true, 60, 0.7f, 2));
    ledger.clear();
    assert(!ledger.stillCurrent(current, true, 60, 0.7f, 2));

    std::cout << "CloudLoader C#: stale slot and bounded OnSpawn queue OK\n";
}
