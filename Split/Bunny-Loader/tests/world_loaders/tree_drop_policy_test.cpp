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
    std::cout<<"CSHARP_TREE_WOOD_AND_ACORN_NATIVE_POLICY=PASS\n";
}
