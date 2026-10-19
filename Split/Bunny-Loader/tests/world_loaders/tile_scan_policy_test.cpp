#include "experiments/csharp/NativeTileScanPolicy.h"
#include <cassert>
#include <iostream>
#include <string_view>

int main() {
    using namespace bl::csharp::tile_scan;
    int r=-1,g=-1,b=-1;
    assert(parseLight("LIGHT|0|200|4000",&r,&g,&b));
    assert(r==0&&g==200&&b==4000);
    assert(parseLight("LIGHT|300|1200|3400",&r,&g,&b));
    assert(r==300&&g==1200&&b==3400);
    for(const std::string_view bad:{
        "", "LIGHT|", "LIGHT|0|0", "LIGHT|0|0|",
        "LIGHT|0|0|0|1", "LIGHT|-1|0|0", "LIGHT|4001|0|0",
        "LIGHT|1.2|0|0", "LIGHT|0|abc|0", "light|0|0|0",
        "LIGHT|2147483648|0|0", "LIGHT||0|0",
        "LIGHT|0|0|0000000000000000000000000000000000000000000000"
    })assert(!parseLight(bad,&r,&g,&b));
    assert(!parseLight("LIGHT|0|0|0",nullptr,&g,&b));
    assert(scanBudget(0)==0);
    assert(scanBudget(17)==17);
    assert(scanBudget(512)==512);
    assert(scanBudget(65536)==512);
    assert(nextCursor(0,0,0)==0);
    assert(nextCursor(32,512,2048)==544);
    assert(nextCursor(2040,512,2048)==504);
    assert(nextCursor(17,17,17)==0);
    assert(nextCursor(7,0,10)==7);
    assert(cellKey(1,2,false)!=cellKey(1,2,true));
    assert(cellKey(1,2,false)!=cellKey(2,1,false));
    assert(cellKey(1,2,false)!=cellKey(1,3,false));
    std::cout<<"CSHARP_TILE_LIGHT_REPLY_NATIVE_POLICY=PASS\n";
}
