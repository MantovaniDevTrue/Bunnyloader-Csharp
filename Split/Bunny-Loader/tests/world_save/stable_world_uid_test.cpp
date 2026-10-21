#include "experiments/csharp/StableWorldUid.h"
#include <cassert>
#include <cstdio>
#include <string>

using bl::csharp::worldsave::stableWorldSystemUid;

int main() {
    const std::string a = stableWorldSystemUid("examplemodcsharp");
    const std::string b = stableWorldSystemUid("examplemodcsharp");
    assert(a == b && a.size() == 36 && a[14] == '8');
    assert(a == "917f103f-6148-8012-8695-801342821bfc");
    assert(a[19] == '8' || a[19] == '9' || a[19] == 'a' || a[19] == 'b');
    assert(stableWorldSystemUid("examplemodcsharp2") != a);
    assert(stableWorldSystemUid("ExampleModCSharp") != a);
    assert(stableWorldSystemUid("example-mod-csharp") != a);
    assert(stableWorldSystemUid("00000000-0000-4000-8000-000000000001") ==
           "00000000-0000-4000-8000-000000000001");
    assert(stableWorldSystemUid("AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA") ==
           "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    assert(stableWorldSystemUid("").empty());
    assert(stableWorldSystemUid("mod/../invalid").empty());
    std::puts("WORLD_STABLE_UID_PASS");
}
