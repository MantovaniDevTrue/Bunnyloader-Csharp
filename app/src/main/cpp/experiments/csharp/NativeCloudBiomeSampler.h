#pragma once

#include "experiments/csharp/NativeCloudBiomePolicy.h"

#include <string>
#include <vector>

namespace bl::csharp {

// Read a bounded snapshot of the LOCAL Terraria player on the game thread.
// This does not consult the JS BiomeLoader and never fabricates mod biome
// flags or block counts. Return false if the game's field layout differs.
bool readNativeCloudVanillaBiomeFacts(bool gameMenu,
                                     NativeCloudBiomeFacts* facts,
                                     std::string* error);

// Reads only the named, registered mod tile counters from the same
// Main.PlayerSceneMetrics._tileCounts used by ExampleModBunny.
// Output is a bounded value snapshot for MonoVM, not a live IL2CPP pointer.
bool readNativeBiomeTileCountsWire(const std::string& owner,
    const std::vector<std::string>& countedTiles,
    std::string* output, std::string* error);

} // namespace bl::csharp
