#include "experiments/csharp/NativeCommandClock.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using bl::csharp::advanceNativeCommandClock;

int main() {
    double time = 0;
    bool day = false;
    assert(advanceNativeCommandClock(0, true, 3600, &time, &day));
    assert(day && time == 3600);
    assert(advanceNativeCommandClock(53999, true, 1, &time, &day));
    assert(!day && time == 0);
    assert(advanceNativeCommandClock(32399, false, 1, &time, &day));
    assert(day && time == 0);
    assert(advanceNativeCommandClock(0, true, -1, &time, &day));
    assert(!day && time == 32399);
    assert(advanceNativeCommandClock(0, false, -1, &time, &day));
    assert(day && time == 53999);
    assert(advanceNativeCommandClock(31000, false, -86400, &time, &day));
    assert(!day && time == 31000);
    assert(advanceNativeCommandClock(1200, true, 86400000, &time, &day));
    assert(day && time == 1200);
    assert(!advanceNativeCommandClock(std::numeric_limits<double>::quiet_NaN(),
                                      true, 4, &time, &day));
    assert(!advanceNativeCommandClock(-2, true, 4, &time, &day));
    assert(!advanceNativeCommandClock(0, true, 86400001, &time, &day));
    assert(!advanceNativeCommandClock(0, true, 1, nullptr, &day));
    std::cout << "BUNNY_CSHARP_NATIVE_COMMAND_CLOCK=PASS" << std::endl;
}
