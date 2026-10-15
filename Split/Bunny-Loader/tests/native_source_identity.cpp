#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include <cassert>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    std::vector<ParsedSourceType> types;
    std::string error;
    const std::string source = R"(
namespace Example {
namespace Items {
public class ExampleWorkbench : BunnyItem {
public override void SetDefaults() {
DisplayName = "Example Workbench";
Texture = "Content/Items/ExampleWorkbench.png";
}
}
}
namespace Projectiles {
public class ExampleWorkbench : BunnyProjectile {
public override void SetDefaults() {
DisplayName = "Projectile";
Texture = "Content/Projectiles/ExampleWorkbench.png";
Width = 12;
Height = 12;
AiStyle = 1;
Friendly = true;
Penetrate = 1;
TimeLeft = 240;
}
}
}
}
)";
    assert(parseCSharpSource(source, &types, &error));
    assert(types.size() == 2);
    assert(types[0].namespaceName == "Example.Items");
    assert(types[1].namespaceName == "Example.Projectiles");
    assert(types[0].name == types[1].name);
    assert(sourceIdentityForBase(types[0].base, types[0].name) == "ITEM:ExampleWorkbench");
    assert(sourceIdentityForBase(types[1].base, types[1].name) == "PROJECTILE:ExampleWorkbench");
    assert(sourceIdentityForWire("WEAPON", "ExampleWorkbench") == "ITEM:ExampleWorkbench");
    assert(sourceIdentityForWire("ACCESSORY", "ExampleWorkbench") == "ITEM:ExampleWorkbench");
    assert(sourceIdentityForWire("TILE", "ExampleWorkbench") == "TILE:ExampleWorkbench");
    assert(sourceIdentityForWire("TILE", "ExampleWorkbench") !=
           sourceIdentityForWire("ITEM", "ExampleWorkbench"));
    types.clear();
    const std::string tileSource = R"(
namespace Example.Tiles;
public class ExampleWorkbench : BunnyTile {
public override void SetStaticDefaults() {
Texture = "Content/Tiles/Furniture/ExampleWorkbench.png";
Flags = BunnyTileFlags.Table | BunnyTileFlags.FrameImportant;
AddMapEntry(200, 200, 200, "ExampleWorkbench");
}
}
)";
    assert(parseCSharpSource(tileSource, &types, &error));
    assert(types.size() == 1);
    assert(types[0].namespaceName == "Example.Tiles");
    assert(types[0].base == "BunnyTile");
    assert(types[0].properties.at("Texture") ==
           "Content/Tiles/Furniture/ExampleWorkbench.png");
    types.clear();
    const std::string scoped = R"(
namespace Example.Tools;
public class Tool : BunnyItem {
public override void SetDefaults() {
DisplayName = "Tool";
Texture = "Content/Items/Tool.png";
}
}
)";
    assert(parseCSharpSource(scoped, &types, &error));
    assert(types.size() == 1);
    assert(types[0].namespaceName == "Example.Tools");
    assert(sourceIdentityForBase("BunnyWeapon", "Tool") ==
           sourceIdentityForBase("BunnyItem", "Tool"));
    return 0;
}
