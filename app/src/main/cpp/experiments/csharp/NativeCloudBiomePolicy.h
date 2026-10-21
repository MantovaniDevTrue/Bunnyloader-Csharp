#pragma once

#include <cmath>
#include <string>

namespace bl::csharp {

// Only values already obtained from the local Terraria player belong here.
// A biome key must never be inferred from height alone when a mod's
// IsBiomeActive also depends on tiles, position or other ModSystem state.
struct NativeCloudBiomeFacts {
    bool playerActive = false;
    bool gameMenu = true;
    bool tenthAnniversaryWorld = false;
    bool zoneSkyHeight = false;
    bool zoneOverworldHeight = false;
    float playerPositionX = 0;
    int maxTilesX = 0;
    int exampleBiomeTileCount = -1;
    bool exampleBiomeRegistered = false;
};

inline bool exampleSurfaceCloudBiomeActive(const NativeCloudBiomeFacts& facts) {
    if (!facts.playerActive || facts.gameMenu ||
        !facts.exampleBiomeRegistered || facts.exampleBiomeTileCount < 40 ||
        facts.maxTilesX <= 0 || facts.maxTilesX > 100000 ||
        !std::isfinite(facts.playerPositionX) ||
        !(facts.zoneSkyHeight || facts.zoneOverworldHeight))
        return false;

    const double tileX = std::floor(static_cast<double>(facts.playerPositionX) / 16.0);
    const double center = static_cast<double>(facts.maxTilesX) / 2.0;
    return std::fabs(tileX - center) <
        static_cast<double>(facts.maxTilesX) / 6.0;
}

inline std::string nativeCloudBiomeKeys(const NativeCloudBiomeFacts& facts) {
    if (!facts.playerActive || facts.gameMenu) return {};
    std::string keys;
    if (facts.zoneSkyHeight)
        keys += "Terraria/ZoneSkyHeight";
    if (facts.zoneOverworldHeight) {
        if (!keys.empty()) keys.push_back(';');
        keys += "Terraria/ZoneOverworldHeight";
    }
    // Do not advertise ExampleSurfaceBiome unless its actual block count
    // has been supplied by the matching C# ModSystem, not by JS flags.
    if (exampleSurfaceCloudBiomeActive(facts)) {
        if (!keys.empty()) keys.push_back(';');
        keys += "ExampleModCSharp/ExampleSurfaceBiome";
    }
    return keys;
}

} // namespace bl::csharp
