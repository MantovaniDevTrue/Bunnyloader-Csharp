#include "experiments/csharp/NativeMountDustId.h"
#include <cassert>
#include <iostream>

int main() {
    using bl::csharp::mount_dust::gemDiamond;
    using bl::csharp::mount_dust::resolveGemDiamond;
    static_assert(gemDiamond==91);
    static_assert(resolveGemDiamond(-1)==91);
    static_assert(resolveGemDiamond(91)==91);
    static_assert(resolveGemDiamond(0)==-1);
    static_assert(resolveGemDiamond(90)==-1);
    static_assert(resolveGemDiamond(92)==-1);
    assert(resolveGemDiamond(-1)==gemDiamond);
    std::cout<<"CSHARP_MOUNT_GEMDIAMOND_DUST_ID_PASS\n";
}
