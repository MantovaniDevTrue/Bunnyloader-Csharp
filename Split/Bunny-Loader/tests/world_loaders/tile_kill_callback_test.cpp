#include "experiments/csharp/NativeTileKillPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_kill;
    bool fail=false,effect=false,noItem=false;
    assert(parseReply("KILL|1|0|1",&fail,&effect,&noItem)&&fail&&!effect&&noItem);
    assert(parseReply("KILL|0|1|0",&fail,&effect,&noItem)&&!fail&&effect&&!noItem);
    for(auto s:{"","KILL|0|1","KILL|2|0|1","KILL|1|0|2","KILL|1|0|0|1",
                "kill|1|0|1","KILL||0|1","KILLS|1|0|1"})
        assert(!parseReply(s,&fail,&effect,&noItem));
    assert(!parseReply("KILL|0|0|0",nullptr,&effect,&noItem));
    assert(!parseReply("KILL|0|0|0",&fail,nullptr,&noItem));
    std::cout<<"CSHARP_TILE_KILL_FLAGS_ORIGINAL_MULTI_CALLBACK=PASS\n";
}
