#include "experiments/csharp/NativeTilePolicy.h"
#include <cassert>

using namespace bl::csharp::tile_policy;

int main() {
    assert(leftOf2x1(10, 0) == 10);
    assert(leftOf2x1(11, 18) == 10);
    assert(leftOf2x1(10, 36) == 10);
    assert(leftOf2x1(11, 54) == 10);
    assert(leftOf2x1(10, -1) == 10);
    assert(!shouldDrop(1, false));
    assert(shouldDrop(1, true));
    assert(shouldDrop(0, false));
    assert(dustBudget(true, 1, 3) == 1);
    assert(dustBudget(false, 1, 3) == 3);
    assert(!incomplete2x1(true, true, true, true));
    assert(incomplete2x1(false, true, true, true));
    assert(incomplete2x1(true, true, false, true));
    assert(incomplete2x1(true, true, true, false));
    return 0;
}
