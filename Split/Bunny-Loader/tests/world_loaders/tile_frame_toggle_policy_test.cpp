#include "experiments/csharp/NativeTileFrameTogglePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_toggle;
    Plan p{};
    assert(plan(2,1,12,9,36,18,&p));
    assert(p.count==6&&p.left==10&&p.top==8&&p.width==3&&p.height==2);
    for(int i=0;i<p.count;++i)assert(p.cells[i].toY-p.cells[i].fromY==36);
    assert(plan(2,1,11,9,18,54,&p));
    for(int i=0;i<p.count;++i)assert(p.cells[i].toY-p.cells[i].fromY==-36);
    assert(plan(3,2,20,14,0,36,&p)&&p.count==3);
    for(int i=0;i<p.count;++i)assert(p.cells[i].toX-p.cells[i].fromX==18);
    assert(plan(3,2,20,14,18,36,&p));
    for(int i=0;i<p.count;++i)assert(p.cells[i].toX-p.cells[i].fromX==-18);
    assert(!plan(3,1,20,14,0,36,&p));
    assert(!plan(2,2,20,14,0,18,&p));
    assert(!plan(2,1,20,14,0,72,&p));
    assert(!plan(3,2,20,14,36,36,&p));
    assert(!plan(2,1,0,0,-1,0,&p));
    assert(!plan(2,1,0,0,36,18,&p));
    assert(allowsRightClick(1));
    assert(!allowsRightClick(0)&&!allowsRightClick(2)&&!allowsRightClick(3));
    assert(lit(0,999,999));
    assert(lit(1,0,0)&&lit(1,36,18));
    assert(!lit(1,0,36)&&!lit(1,36,54));
    assert(lit(2,0,0)&&!lit(2,18,0));
    assert(!lit(2,36,0)&&!lit(3,0,0));
    std::cout<<"CSHARP_TILE_FRAME_TOGGLE_PLAN=PASS\n";
}
