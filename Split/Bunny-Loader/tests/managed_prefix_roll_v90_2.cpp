#include "experiments/csharp/ManagedPrefixRollSelection.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

int main() {
    const int32_t candidates[] = {1, 2, 100, 100, 100, 100, 100};
    using bl::csharp::weightedPrefixCandidate;
    assert(weightedPrefixCandidate(candidates, 7, 0.0) == 1);
    assert(weightedPrefixCandidate(candidates, 7, 1.0 / 7) == 2);
    assert(weightedPrefixCandidate(candidates, 7, 2.0 / 7) == 100);
    assert(weightedPrefixCandidate(candidates, 7, 0.9999999) == 100);
    assert(weightedPrefixCandidate(candidates, 7, 1.0) == -1);
    assert(weightedPrefixCandidate(candidates, 7, -0.01) == -1);
    assert(weightedPrefixCandidate(candidates, 7,
           std::numeric_limits<double>::quiet_NaN()) == -1);
    assert(weightedPrefixCandidate(nullptr, 0, 0.5) == -1);
    for (int i = 0; i < 7000; ++i) {
        const int selected = weightedPrefixCandidate(
            candidates, 7, (i + 0.5) / 7000.0);
        assert(selected == 1 || selected == 2 || selected == 100);
    }
}
