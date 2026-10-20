#include "experiments/csharp/NativeTreeDropPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_drop;
    assert(valid(-1,true)&&valid(-1,false)&&valid(6196,true));
    assert(!valid(0,true)&&!valid(6195,false)&&!valid(65536,true));
    int main=9,other=27;
    apply(true,27,6200,false,&main,&other);
    assert(main==6200&&other==0);
    main=9;other=28;
    apply(true,27,6200,false,&main,&other);
    assert(main==6200&&other==28);
    main=9;other=27;
    apply(false,27,6200,true,&main,&other);
    assert(main==9&&other==27);
    main=9;other=0;
    apply(true,27,-1,true,&main,&other);
    assert(main==9&&other==0);
    main=9;other=27;
    apply(true,-1,6200,false,&main,&other);
    assert(main==6200&&other==27);
    assert(shouldAddTopAcorn(0,27,true,22,198,0,0.0));
    assert(shouldAddTopAcorn(0,27,true,44,220,2,0.49999));
    assert(!shouldAddTopAcorn(0,27,true,22,198,0,0.5));
    assert(!shouldAddTopAcorn(27,27,true,22,198,0,0.0));
    assert(!shouldAddTopAcorn(0,27,false,22,198,0,0.0));
    assert(!shouldAddTopAcorn(0,27,true,21,198,0,0.0));
    assert(!shouldAddTopAcorn(0,27,true,22,197,0,0.0));
    assert(!shouldAddTopAcorn(0,27,true,22,198,1,0.0));
    assert(!shouldAddTopAcorn(0,-1,true,22,198,0,0.0));
    assert(!shouldAddTopAcorn(0,27,true,22,198,0,-0.01));
    std::cout<<"CSHARP_TREE_WOOD_AND_ACORN_NATIVE_POLICY=PASS\n";
}
