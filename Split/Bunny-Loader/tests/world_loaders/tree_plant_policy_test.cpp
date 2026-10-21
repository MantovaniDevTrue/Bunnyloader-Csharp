#include "experiments/csharp/NativeTreePlantPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_plant;
    int t=20,s=9;
    assert(rewrite(20,754,754,801,&t,&s));
    assert(t==801&&s==0);
    t=20;s=7;
    assert(!rewrite(21,754,754,801,&t,&s));
    assert(t==20&&s==7);
    assert(!rewrite(20,755,754,801,&t,&s));
    assert(t==20&&s==7);
    assert(!rewrite(20,754,754,20,&t,&s));
    assert(!rewrite(20,754,754,65536,&t,&s));
    assert(!rewrite(20,754,754,801,nullptr,&s));
    assert(!rewrite(20,754,754,801,&t,nullptr));
    assert(!valid(753,801));
    assert(!valid(754,753));
    assert(valid(754,801));
    std::cout<<"CSHARP_TREE_ACORN_TO_SAPLING_NATIVE_POLICY=PASS\n";
}
