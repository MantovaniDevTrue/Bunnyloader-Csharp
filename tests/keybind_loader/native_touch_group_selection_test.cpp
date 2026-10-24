#include "experiments/csharp/NativeTouchGroupSelection.h"
#include <cassert>

using bl::csharp::touch::kGroupReadError;
using bl::csharp::touch::nativeTouchGroupSelected;

int main() {
    assert(nativeTouchGroupSelected(0, -1, 10));
    assert(nativeTouchGroupSelected(7, -1, 10));
    assert(!nativeTouchGroupSelected(0, kGroupReadError, 10));
    assert(!nativeTouchGroupSelected(0, 0, 10));
    assert(!nativeTouchGroupSelected(0, 1, 10));
    assert(!nativeTouchGroupSelected(-1, -1, 10));
    assert(!nativeTouchGroupSelected(kGroupReadError, -1, 10));
    assert(!nativeTouchGroupSelected(9, -1, 16));
    assert(!nativeTouchGroupSelected(10, -1, 10));
    assert(!nativeTouchGroupSelected(0, -1, 0));
    return 0;
}
