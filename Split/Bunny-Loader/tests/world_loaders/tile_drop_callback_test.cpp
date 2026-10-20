#include "experiments/csharp/NativeTileDropPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_drop;
    Drops d{};
    assert(parseReply("DEFAULT",&d)&&!d.custom);
    assert(parseReply("DROPS|0",&d)&&d.custom&&d.first.type==0);
    assert(parseReply("DROPS|1|40,1",&d)&&d.first.type==40&&d.first.stack==1&&d.second.type==0);
    assert(parseReply("DROPS|2|40,1|41,5",&d)&&d.first.type==40&&d.second.type==41&&d.second.stack==5);
    for(auto s:{"","DROPS|","DROPS|3","DROPS|1","DROPS|1|0,1","DROPS|1|40,0",
                "DROPS|1|65536,1","DROPS|1|40,10000","DROPS|2|40,1",
                "DROPS|1|40,1|41,1","DROPS|2|40,1|41,2|42,1","DROPS|2|40,x|41,3"})
        assert(!parseReply(s,&d));
    assert(!parseReply("DEFAULT",nullptr));
    std::cout<<"CSHARP_TILE_TWO_ITEM_DROPS_AND_DEFAULT_FALLBACK=PASS\n";
}
