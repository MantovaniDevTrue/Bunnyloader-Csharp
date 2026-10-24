#include "experiments/csharp/NativeTreeTexturePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tree_texture;
    assert(validStem("Content/Tiles/Plants/ExampleTree"));
    assert(validStem("Content/Trees/My-Tree"));
    assert(!validStem(""));
    assert(!validStem("/Content/Trees/Test"));
    assert(!validStem("Content//Trees/Test"));
    assert(!validStem("Content/Trees/"));
    assert(!validStem("Content/Trees/../Test"));
    assert(!validStem("Content\\Trees\\Test"));
    assert(!validStem("Content/Trees/Test.png"));
    assert(validDimensions(176,220,244,80,82,124));
    assert(!validDimensions(175,220,244,80,82,124));
    assert(!validDimensions(176,220,79,80,82,124));
    assert(!validDimensions(176,220,244,80,79,124));
    assert(!validDimensions(176,220,244,80,82,4097));
    auto refs=paths("/mods/example","Content/Tiles/Plants/ExampleTree");
    assert(refs[0]=="/mods/example/content/Content/Tiles/Plants/ExampleTree.png");
    assert(refs[1]=="/mods/example/content/Content/Tiles/Plants/ExampleTree_Tops.png");
    assert(refs[2]=="/mods/example/content/Content/Tiles/Plants/ExampleTree_Branches.png");
    assert(paths("/mods/example","../Escape")[0].empty());
    std::cout<<"CSHARP_TREE_NATIVE_THREE_TEXTURE_POLICY=PASS\n";
}
