#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include "experiments/csharp/ManagedSourceClassCount.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main() {
    using namespace bl::csharp;
    std::vector<ParsedSourceType> definitions;
    std::string error;
    for(const char* path:{
        "samples/ExampleModCSharp/Playable/content/Content/Tiles/Plants/ExampleTree.cs",
        "samples/ExampleModCSharp/Playable/content/Content/Tiles/Plants/ExampleSapling.cs",
        "samples/ExampleModCSharp/Playable/content/Content/Gores/ExampleTree_Leaf.cs"}) {
        std::ifstream input(path);assert(input.good());
        std::string source((std::istreambuf_iterator<char>(input)),{});
        assert(parseCSharpSource(source,&definitions,&error));
    }
    assert(definitions.size()==3);
    SourceModStatus mod{};
    for(const auto& def:definitions) {
        assert(!sourceIdentityForBase(def.base,def.name).empty());
        if(def.base=="BunnyTree") { ++mod.trees;assert(!sourceRequiresNativeRegistration(def.base)); }
        else if(def.base=="BunnyGore") { ++mod.gores;assert(!sourceRequiresNativeRegistration(def.base)); }
        else { assert(def.base=="BunnyTile");++mod.tiles;assert(sourceRequiresNativeRegistration(def.base)); }
    }
    assert(mod.trees==1 && mod.gores==1 && mod.tiles==1);
    assert(expectedManagedSourceTypes(mod)==3);
    assert(sourceIdentityForWire("TREE","ExampleTree")==sourceIdentityForBase("BunnyTree","ExampleTree"));
    assert(sourceIdentityForWire("GORE","ExampleTree_Leaf")==sourceIdentityForBase("BunnyGore","ExampleTree_Leaf"));
    mod.commands=1;mod.worldSystems=1;
    assert(expectedManagedSourceTypes(mod)==4); // World systems use their separate dispatcher/count contract.
    std::cout<<"TREE_GORE_REAL_SOURCE_PARSER_IDENTITY_AND_NATIVE_CLASS_COUNT=PASS\n";
}
