#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionPeerPolicy.h"

#include <cassert>
#include <cstdint>

int main() {
    using namespace bl::csharp::invasion_peer;
    static_assert(sweepInterval > 0 && sweepInterval < expiryInterval);
    assert(!expired(0, 0));
    assert(!expired(300, 0));
    assert(!expired(expiryInterval, 0));
    assert(expired(expiryInterval + 1, 0));
    assert(!expired(2100, 1800));
    assert(!expired(3600, 1800));
    assert(expired(3601, 1800));
    assert(!expired(1, 2));
    return 0;
}
