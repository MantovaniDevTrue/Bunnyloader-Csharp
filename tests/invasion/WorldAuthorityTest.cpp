#include "../../app/src/main/cpp/experiments/csharp/WorldAuthority.h"

#include <cassert>

int main() {
    using bl::csharp::worldIsAuthoritative;
    static_assert(worldIsAuthoritative(0));
    static_assert(!worldIsAuthoritative(1));
    static_assert(worldIsAuthoritative(2));
    static_assert(worldIsAuthoritative(3));
    static_assert(!worldIsAuthoritative(-1));
    static_assert(!worldIsAuthoritative(4));
    assert(worldIsAuthoritative(0));
    assert(!worldIsAuthoritative(1));
    assert(worldIsAuthoritative(2));
    assert(worldIsAuthoritative(3));
    assert(!worldIsAuthoritative(-1));
    return 0;
}
