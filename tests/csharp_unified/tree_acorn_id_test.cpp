#include "experiments/csharp/NativeTreeAcornId.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::tree_acorn;
    static_assert(vanillaId==27);
    static_assert(resolveId(-1)==27);
    static_assert(resolveId(27)==27);
    static_assert(resolveId(26)==-1);
    static_assert(resolveId(28)==-1);
    assert(resolveId(-1)==vanillaId);
    std::cout<<"CSHARP_TREE_ACORN_ID_PASS\n";
}
