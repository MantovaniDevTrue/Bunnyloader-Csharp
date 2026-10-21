#include "experiments/csharp/NativeBiomeManifest.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    std::vector<NativeBiomeDescriptor> registry;
    std::string error;
    const std::string alpha = "a2b3-a0";
    assert(appendNativeBiomeManifest(alpha,
        "BUNNY_BIOMES_V1\n"
        "BIOME\tExampleSurfaceBiome\tExampleTile\n"
        "BIOME\tSkyBiome\t\n", &registry, &error));
    assert(registry.size() == 2);
    assert(registry[0].tileNames.size() == 1);
    assert(registry[0].tileNames[0] == "ExampleTile");
    assert(registry[1].tileNames.empty());
    assert(appendNativeBiomeManifest("beta",
        "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\tOtherTile\n",
        &registry, &error));
    assert(registry.size() == 3);

    auto rejectManifest = [&](std::string wire) {
        auto before = registry;
        assert(!appendNativeBiomeManifest("alpha", wire, &registry, &error));
        assert(registry.size() == before.size() && !error.empty());
        for (size_t i = 0; i < registry.size(); ++i)
            assert(registry[i].className == before[i].className);
    };
    rejectManifest("BUNNY_BIOMES_V0\nBIOME\tOther\tTile\n");
    rejectManifest("BUNNY_BIOMES_V1\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\tExampleTile\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tSecond\tTile\nBIOME\tSecond\tTile\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tBad-Name\tTile\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tNew\tExampleTile,ExampleTile\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tNew\tTile,\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tNew\tTile\tExtra\n");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tNew\tTile");
    rejectManifest("BUNNY_BIOMES_V1\nBIOME\tNew\t../Tile\n");

    std::vector<std::string> active;
    assert(appendNativeBiomeActiveKeys(registry, alpha,
        "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\n",
        false, &active, &error));
    assert(active.size() == 1 && active[0] == alpha + "/ExampleSurfaceBiome");
    assert(appendNativeBiomeActiveKeys(registry, "beta",
        "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\n",
        false, &active, &error));
    assert(active.size() == 2 && active[1] == "beta/ExampleSurfaceBiome");
    auto rejectActive = [&](const std::string& owner, const std::string& wire,
                            bool menu = false) {
        auto previous = active;
        assert(!appendNativeBiomeActiveKeys(registry, owner, wire,
                                           menu, &active, &error));
        assert(active == previous && !error.empty());
    };
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\n");
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tUnknown\n");
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\tOther\n");
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tBad-Name\n");
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tSkyBiome");
    rejectActive(alpha, "BUNNY_BIOMES_V1\nBIOME\tSkyBiome\n", true);
    assert(appendNativeBiomeActiveKeys(registry, alpha,
        "BUNNY_BIOMES_V1\n", true, &active, &error));
    assert(active.size() == 2);

    std::cout << "BunnyBiome CSharp native manifests and owned keys OK\n";
}
