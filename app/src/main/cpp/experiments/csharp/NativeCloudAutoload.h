#pragma once

#include "experiments/csharp/NativeCloudRegistry.h"

#include <string>

namespace bl::csharp {

// Original ModFiles.ListTextures('Clouds') sees Clouds/*.png in any content
// directory (and the legacy Assets/Textures/Clouds directory). Scan only
// once during registration; never from addCloud or a render callback.
// Existing class names win, matching CloudLoader.Autoload in ExampleModBunny.
// If the filesystem batch is invalid, the previous registry stays untouched.
bool appendNativeCloudAutoload(NativeCloudRegistry* registry,
                               const std::string& modId,
                               const std::string& modDirectory,
                               std::string* error);

} // namespace bl::csharp
