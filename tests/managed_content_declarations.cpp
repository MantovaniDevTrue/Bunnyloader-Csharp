#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>

using namespace bl::csharp;

int main(int argc, char** argv) {
    std::vector<ParsedSourceType> types;
    std::string error;
    assert(parseCSharpSource(R"(
        using BunnyLoader.Source;
        namespace Independent;
        public abstract class MaterialBase<T> : BunnyItem {
            public override void SetDefaults() { MaxStack = 9999; }
        }
        [BunnyContent(BunnyContentKind.Item, "Material", "Content/Items/Material.png")]
        public sealed class Material : MaterialBase<int> {
            public override void SetDefaults() { base.SetDefaults(); Damage = 12 + 3; }
        }
    )", &types, &error));
    assert(types.size() == 1 && types[0].base == "BunnyItem");
    assert(types[0].namespaceName == "Independent");
    assert(types[0].properties.at("Texture") == "Content/Items/Material.png");
    assert(types[0].properties.at("DisplayName") == "Material");
    types.clear();
    assert(parseCSharpSource(R"(
        [BunnyContent(BunnyContentKind.Tile, "Bench", "Content/Tiles/Bench.png")]
        public sealed class Bench : SharedTileBase {
            public override void SetStaticDefaults() { Shape = BunnyTileShape.Style2x1; }
        }
    )", &types, &error));
    assert(types.size() == 1 && types[0].base == "BunnyTile");
    types.clear();
    assert(!parseCSharpSource(R"(
        [BunnyContent(BunnyContentKind.Tile, "Item", "Item.png")]
        public sealed class Item : BunnyItem { public override void SetDefaults() {} }
    )", &types, &error));
    if (argc >= 2) {
        types.clear();
        for (const std::string path : {"main.cs", "Content/Items/DeclaredMaterial.cs"}) {
            std::ifstream file(std::string(argv[1]) + "/content/" + path);
            assert(file);
            const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            assert(parseCSharpSource(source, &types, &error, false));
        }
        assert(types.size() == 1 && types[0].name == "DeclaredMaterial");
        assert(types[0].properties.at("Texture") == "Content/Items/DeclaredMaterial.png");
    }
    assert(error.find("difere da classe base") != std::string::npos);
    assert(!parseCSharpSource(R"(
        [BunnyContent(BunnyContentKind.Item, "Item", "Item.png")]
        public sealed class Item : SharedBase {
            public override void SetDefaults() { Texture = "Other.png"; }
        }
    )", &types, &error));
    assert(!parseCSharpSource(R"(
        [BunnyContent(BunnyContentKind.Item, MakeName(), "Item.png")]
        public sealed class Item : SharedBase { public override void SetDefaults() {} }
    )", &types, &error));
    if(argc==3) {
        types.clear();error.clear();
        std::ifstream file(std::string(argv[2])+"/content/main.cs");assert(file);
        const std::string source{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        assert(parseCSharpSource(source,&types,&error));
        assert(types.size()==1&&types[0].base=="BunnyItem"&&types[0].name=="NativeCharm");
        assert(types[0].properties.at("Texture")=="Content/Items/NativeCharm.png");
        std::ifstream texture(std::string(argv[2])+"/content/"+types[0].properties.at("Texture"));assert(texture);
    }
    std::cout << "BUNNY_NATIVE_CONTENT_DECLARATIONS=PASS\n";
}
