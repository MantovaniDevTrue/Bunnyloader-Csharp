#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    const std::string sample = R"CS(
using BunnyLoader.Source;
namespace ExampleModCSharp.Biomes;
public sealed class ExampleSurfaceBiome : BunnyBiome {
    public override System.Collections.Generic.IReadOnlyList<string> CountedTiles =>
        new[] { "ExampleTile" };
    public override bool IsBiomeActive(BunnyBiomeSnapshot snapshot) =>
        snapshot.TileCount("ExampleTile") >= 40 &&
        snapshot.IsCenterSurface;
}
)CS";
    std::vector<ParsedSourceType> parsed;
    std::string error;
    assert(parseCSharpSource(sample, &parsed, &error, true));
    assert(parsed.size() == 1);
    assert(parsed[0].base == "BunnyBiome");
    assert(parsed[0].name == "ExampleSurfaceBiome");
    assert(sourceContentDomain("BunnyBiome") == "BIOME");
    assert(sourceIdentityForBase("BunnyBiome", "ExampleSurfaceBiome") ==
           "BIOME:ExampleSurfaceBiome");
    assert(sourceRequiresNativeRegistration("BunnyBiome"));
    assert(sourceIdentityForBase("BunnyCloud", "ExampleSurfaceBiome") ==
           "CLOUD:ExampleSurfaceBiome");
    assert(sourceIdentityForBase("BunnyBiome", "ExampleSurfaceBiome") !=
           sourceIdentityForBase("BunnyCloud", "ExampleSurfaceBiome"));

    parsed.clear();
    error.clear();
    assert(parseCSharpSource(
        "public abstract class BaseBiome : BunnyBiome {}",
        &parsed, &error, false));
    assert(parsed.empty());
    std::cout << "BunnyBiome CSharp parser and identity isolation OK\n";
}
