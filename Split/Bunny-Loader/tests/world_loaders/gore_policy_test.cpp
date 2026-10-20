#include "experiments/csharp/NativeGorePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::gore_policy;
    assert(valid("ExampleTree_Leaf","Content/Gores/ExampleTree_Leaf",3));
    assert(valid("Leaf","Gores/Leaf",0));
    assert(!valid("Bad.Name","Gores/Leaf",3));
    assert(!valid("Leaf","../Gores/Leaf",3));
    assert(!valid("Leaf","Content//Leaf",3));
    assert(!valid("Leaf","Content/Gores/Leaf.png",3));
    assert(!valid("Leaf","Gores/Leaf",256));
    assert(filePath("/mods/example","Content/Gores/ExampleTree_Leaf")==
           "/mods/example/content/Content/Gores/ExampleTree_Leaf.png");
    assert(filePath("/mods/example","../bad").empty());
    assert(capacity(1000,1)&&capacity(32766,1));
    assert(!capacity(32766,2)&&!capacity(2,257)&&!capacity(0,1));
    std::cout<<"CSHARP_MOD_GORE_POLICY=PASS\n";
}