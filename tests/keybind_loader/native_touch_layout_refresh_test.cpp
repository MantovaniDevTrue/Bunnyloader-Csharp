#include "experiments/csharp/NativeTouchLayoutRefresh.h"
#include <cassert>

using bl::csharp::touch::needsNativeTouchLayoutRefresh;

int main() {
    int first = 1;
    int second = 2;
    assert(!needsNativeTouchLayoutRefresh(true, false, 10, 10, &first, &first));
    assert(needsNativeTouchLayoutRefresh(false, false, 10, 10, &first, &first));
    assert(needsNativeTouchLayoutRefresh(true, true, 10, 10, &first, &first));
    assert(needsNativeTouchLayoutRefresh(true, false, 10, 11, &first, &first));
    assert(needsNativeTouchLayoutRefresh(true, false, 10, 10, &first, &second));
    assert(needsNativeTouchLayoutRefresh(true, false, 10, 10, &first, nullptr));
    assert(!needsNativeTouchLayoutRefresh(true, false, 0, 0, nullptr, nullptr));
    return 0;
}
