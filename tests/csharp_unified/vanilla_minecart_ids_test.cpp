#include "experiments/csharp/VanillaMinecartIds.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp::vanilla_minecarts;
    static_assert(meowmere==33);
    static_assert(fart==51);
    static_assert(resolveId(meowmere,-1)==33);
    static_assert(resolveId(fart,-1)==51);
    static_assert(resolveId(meowmere,33)==33);
    static_assert(resolveId(fart,51)==51);
    static_assert(resolveId(meowmere,51)==-1);
    static_assert(resolveId(fart,33)==-1);
    assert(resolveId(meowmere,-1)==meowmere);
    assert(resolveId(fart,-1)==fart);
    std::cout<<"CSHARP_VANILLA_MINECART_IDS_PASS\n";
}
