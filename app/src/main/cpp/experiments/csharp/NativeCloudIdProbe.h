#pragma once

#include "experiments/csharp/NativeCloudIdPlan.h"

#include <string>

namespace bl::csharp {

// Inspection ONLY. The caller must prove that JS Ready/setup and the mobile
// TextureMaskManager.LoadMasks stage have both finished. This function never
// alters Terraria fields, installs hooks or marks C# content ready.
NativeCloudIdPlan probeNativeCloudIdPlan(int cloudCount,
                                        bool jsReadySetupFinished,
                                        bool mobileLoadMasksFinished);

} // namespace bl::csharp
