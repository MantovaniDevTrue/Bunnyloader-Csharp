#include "experiments/csharp/NativeTileDustPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_dust;
    int n=-100;bool allowed=false;
    assert(parseCount("DUST|7",&n)&&n==7);
    assert(parseCount("DUST|0",&n)&&n==0);
    assert(parseCount("DUST|20",&n)&&n==20);
    for(auto s:{"","DUST|","DUST|-1","DUST|21","DUST|7bad","DUST|2147483648"})
        assert(!parseCount(s,&n));
    assert(parseCreate("CREATE|1|57",&allowed,&n)&&allowed&&n==57);
    assert(parseCreate("CREATE|0|-1",&allowed,&n)&&!allowed&&n==-1);
    for(auto s:{"","CREATE|2|7","CREATE|1|-2","CREATE|0|2000","CREATE|0|",
                "CREATE|1|7|1","CREATE|1|99999999","create|1|1"})
        assert(!parseCreate(s,&allowed,&n));
    assert(!parseCreate("CREATE|1|1",nullptr,&n));
    std::cout<<"CSHARP_TILE_DUST_COUNT_AND_CREATE_NATIVE_POLICY=PASS\n";
}
