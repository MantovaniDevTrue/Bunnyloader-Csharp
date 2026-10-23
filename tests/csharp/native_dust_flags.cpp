#include "experiments/csharp/ManagedDustFlagMask.h"

#include <cassert>

using bl::csharp::managedDustFlagsValid;

int main() {
    assert(managedDustFlagsValid(0, 1u|32u, true));
    assert(managedDustFlagsValid(0, 1u, false));
    assert(!managedDustFlagsValid(0, 1u|2u|32u, true));
    assert(managedDustFlagsValid(1, 1u|2u|16u|32u, true));
    assert(!managedDustFlagsValid(1, 1u|4u|32u, true));
    assert(managedDustFlagsValid(2, 1u|2u|4u|16u|32u, true));
    assert(!managedDustFlagsValid(2, 1u|8u|32u, true));
    assert(managedDustFlagsValid(3, 1u|2u|8u|16u|32u, true));
    assert(!managedDustFlagsValid(3, 1u|4u|32u, true));
    assert(managedDustFlagsValid(4, 1u|2u|32u, true));
    assert(!managedDustFlagsValid(4, 1u|8u|32u, true));
    assert(managedDustFlagsValid(5, 1u, false));
    assert(!managedDustFlagsValid(5, 1u|32u, true));
    assert(!managedDustFlagsValid(1, 32u, true));
    assert(!managedDustFlagsValid(-1, 1u, false));
    assert(!managedDustFlagsValid(6, 1u, false));
    assert(!managedDustFlagsValid(0, 1u, true));
}
