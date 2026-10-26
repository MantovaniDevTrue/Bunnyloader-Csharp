#include "il2cpp/NativePointerContract.h"

#include <cassert>
#include <cstdint>

int main() {
    using bl::il2cpp::plausibleManagedPointerValue;
    assert(!plausibleManagedPointerValue(0, true));
    assert(!plausibleManagedPointerValue(UINT64_C(0x3F80000000000000), true));
    assert(!plausibleManagedPointerValue(UINT64_C(0x0080000000000000), true));
    assert(!plausibleManagedPointerValue(UINT64_C(0x0000000000001000), true));
    assert(!plausibleManagedPointerValue(UINT64_C(0x0000007341F00001), true));
    assert(plausibleManagedPointerValue(UINT64_C(0xB400007351F85FC0), true));
    assert(plausibleManagedPointerValue(UINT64_C(0x0000007341F0C440), true));
    assert(plausibleManagedPointerValue(UINT64_C(0x0000007341F0C440), false));
}
