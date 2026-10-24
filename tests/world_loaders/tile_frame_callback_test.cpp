#include "experiments/csharp/NativeTileFramePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_frame;
    bool run=false,reset=false,noBreak=false;
    assert(parseReply("FRAME|1|0|1",&run,&reset,&noBreak)&&run&&!reset&&noBreak);
    assert(parseReply("FRAME|0|1|0",&run,&reset,&noBreak)&&!run&&reset&&!noBreak);
    for(auto reply:{"","FRAME|0|1","FRAME|2|0|0","FRAME|1|9|0",
                    "FRAME|1|0|2","FRAME|1|1|1|0","frame|1|1|1",
                    "FRAME||1|0","FRAMES|1|0|1"})
        assert(!parseReply(reply,&run,&reset,&noBreak));
    assert(!parseReply("FRAME|1|0|0",nullptr,&reset,&noBreak));
    assert(!parseReply("FRAME|1|0|0",&run,nullptr,&noBreak));
    assert(!parseReply("FRAME|1|0|0",&run,&reset,nullptr));
    std::cout<<"CSHARP_TILE_FRAME_OVERRIDE_CANCEL_RESET_NOBREAK=PASS\n";
}
