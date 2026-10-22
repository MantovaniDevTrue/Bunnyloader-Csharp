#include "experiments/csharp/NativeCloudBiomePolicy.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

int main() {
    using namespace bl::csharp;
    NativeCloudBiomeFacts facts{};
    facts.gameMenu = false;
    facts.playerActive = true;
    facts.zoneOverworldHeight = true;
    facts.playerPositionX = 3200 * 16.f;
    facts.maxTilesX = 6400;

    // The original ExampleBiomeTileCount is not exposed to the C# host yet.
    // A surface location ALONE must never announce a mod biome.
    assert(facts.exampleBiomeTileCount == -1);
    assert(!exampleSurfaceCloudBiomeActive(facts));
    assert(nativeCloudBiomeKeys(facts) == "Terraria/ZoneOverworldHeight");

    facts.exampleBiomeTileCount = 39;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    facts.exampleBiomeTileCount = 40;
    // A matching C# tile can exist without a registered C# ModBiome.
    // Never use tile count alone to claim that biome exists.
    assert(!exampleSurfaceCloudBiomeActive(facts));
    facts.exampleBiomeRegistered = true;
    assert(exampleSurfaceCloudBiomeActive(facts));
    assert(nativeCloudBiomeKeys(facts) ==
        "Terraria/ZoneOverworldHeight;ExampleModCSharp/ExampleSurfaceBiome");

    facts.zoneOverworldHeight = false;
    facts.zoneSkyHeight = true;
    assert(exampleSurfaceCloudBiomeActive(facts));
    assert(nativeCloudBiomeKeys(facts) ==
        "Terraria/ZoneSkyHeight;ExampleModCSharp/ExampleSurfaceBiome");
    facts.zoneOverworldHeight = true;
    assert(nativeCloudBiomeKeys(facts) ==
        "Terraria/ZoneSkyHeight;Terraria/ZoneOverworldHeight;"
        "ExampleModCSharp/ExampleSurfaceBiome");

    facts.playerPositionX = 0;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    facts.playerPositionX = 3200 * 16.f;
    facts.zoneSkyHeight = false;
    facts.zoneOverworldHeight = false;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    facts.zoneOverworldHeight = true;
    facts.gameMenu = true;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    assert(nativeCloudBiomeKeys(facts).empty());
    facts.gameMenu = false;
    facts.playerActive = false;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    assert(nativeCloudBiomeKeys(facts).empty());
    facts.playerActive = true;
    facts.playerPositionX = std::numeric_limits<float>::quiet_NaN();
    assert(!exampleSurfaceCloudBiomeActive(facts));
    facts.playerPositionX = 3200 * 16.f;
    facts.maxTilesX = 0;
    assert(!exampleSurfaceCloudBiomeActive(facts));
    std::cout << "CloudLoader C#: authentic surface biome conditions and unknown tile count gate OK\n";
}
