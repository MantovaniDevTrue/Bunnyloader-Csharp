#pragma once

#include "experiments/csharp/NativeCloudBiomePolicy.h"

#include <string>

namespace bl::csharp {

// Read a bounded snapshot of the LOCAL Terraria player on the game thread.
// This does not consult the JS BiomeLoader and never fabricates mod biome
// flags or block counts. Return false if the game's field layout differs.
bool readNativeCloudVanillaBiomeFacts(bool gameMenu,
                                     NativeCloudBiomeFacts* facts,
                                     std::string* error);

} // namespace bl::csharp
