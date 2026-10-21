#include "experiments/csharp/NativeTouchActionIds.h"
#include <cassert>
#include <cstdio>
using namespace bl::csharp::touch;

int main() {
    auto ids = resolveActionIds(28, 33, 35, 128);
    assert(ids.valid());
    assert(ids.action(0) > 28 + kOtherLoaderReservation);
    assert(ids.control(0) >= 33 + kOtherLoaderReservation);
    assert(ids.quick(0) > 100 + kOtherLoaderReservation - 1);
    assert(ids.action(0) != ids.control(0));
    assert(ids.action(0) == 541);
    assert(ids.control(0) == 545);
    assert(ids.quick(0) == 612);
    for (int i = 0; i < 128; ++i) {
        assert(ids.byAction(ids.action(i)) == i);
        assert(ids.byControl(ids.control(i)) == i);
        assert(ids.byQuick(ids.quick(i)) == i);
        assert(ids.action(i) != ids.quick(i));
    }
    assert(ids.action(-1) == -1 && ids.control(128) == -1);
    assert(ids.byAction(0) == -1 && ids.byControl(33) == -1);
    assert(ids.byQuick(100) == -1 && ids.byAction(670) == -1);
    assert(!resolveActionIds(-1, 33, 35, 128).valid());
    assert(!resolveActionIds(28, -1, 35, 128).valid());
    assert(!resolveActionIds(28, 33, -1, 128).valid());
    assert(!resolveActionIds(28, 33, 35, 0).valid());
    assert(!resolveActionIds(28, 33, 35, 129).valid());
    assert(!resolveActionIds(3000, 3000, 3000, 128).valid());
    std::puts("native touch action ID namespace PASS");
}
