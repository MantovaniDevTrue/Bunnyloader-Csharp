#include "experiments/csharp/SourceParser.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using bl::csharp::ParsedSourceType;

int main() {
    const std::string source = R"(using BunnyLoader.Source;
namespace ExampleModCSharp;
public sealed class ExampleSlimeNPC : BunnyNPC {
    public override void SetDefaults() {
        DisplayName = "Example Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 32;
        Height = 24;
        Damage = 7;
        Defense = 2;
        LifeMax = 25;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
        Banner = "ExampleSlimeNPC";
        BannerItem = "ExampleSlimeBanner";
    }
})";
    std::vector<ParsedSourceType> defs;
    std::string error;
    assert(bl::csharp::parseCSharpSource(source, &defs, &error));
    assert(defs.size() == 1);
    assert(defs.front().name == "ExampleSlimeNPC");
    assert(defs.front().base == "BunnyNPC");
    assert(defs.front().properties.at("Banner") == "ExampleSlimeNPC");
    assert(defs.front().properties.at("BannerItem") == "ExampleSlimeBanner");

    defs.clear();
    error.clear();
    const std::string bannerTile = R"(using BunnyLoader.Source;
namespace ExampleModCSharp;
public sealed class ExampleEnemyBannerTile : BunnyBannerTile {
    public override void SetStaticDefaults() {
        base.SetStaticDefaults();
        Texture = "Content/Tiles/Banners/ExampleEnemyBannerTile.png";
        DisplayName = "Example Slime Banner";
    }
})";
    assert(bl::csharp::parseCSharpSource(bannerTile, &defs, &error));
    assert(defs.size() == 1);
    assert(defs.front().name == "ExampleEnemyBannerTile");
    assert(defs.front().base == "BunnyTile");
    assert(defs.front().properties.at("Texture") ==
           "Content/Tiles/Banners/ExampleEnemyBannerTile.png");

    defs.clear();
    error.clear();
    auto bad = source;
    auto pos = bad.find("BannerItem = \"ExampleSlimeBanner\"");
    assert(pos != std::string::npos);
    bad.replace(pos, std::string("BannerItem = \"ExampleSlimeBanner\"").size(),
                "BannerItem = SomeRuntimeLookup()");
    assert(!bl::csharp::parseCSharpSource(bad, &defs, &error));
    assert(!error.empty());
    std::cout << "Banner early CSharp parser PASS\n";
}
