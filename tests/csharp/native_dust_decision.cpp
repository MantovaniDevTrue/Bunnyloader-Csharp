#include "experiments/csharp/ManagedDustDecision.h"

#include <cassert>
#include <cstdint>
#include <initializer_list>

using bl::csharp::managedDustDecisionCurrent;

int main() {
    constexpr std::uint32_t white = 0xffaabbccu;
    constexpr std::uint32_t dark = 0xff112233u;
    assert(managedDustDecisionCurrent(0, 300, 296, white, dark));
    assert(!managedDustDecisionCurrent(0, 300, 301, white, white));
    for (int phase : {1, 2, 3, 4}) {
        assert(managedDustDecisionCurrent(phase, 300, 300, white, white));
        assert(!managedDustDecisionCurrent(phase, 301, 300, white, white));
        assert(!managedDustDecisionCurrent(phase, 299, 300, white, white));
    }
    assert(!managedDustDecisionCurrent(3, 300, 300, white, dark));
    assert(managedDustDecisionCurrent(4, 300, 300, white, dark));
    assert(!managedDustDecisionCurrent(5, 300, 300, white, white));
}
