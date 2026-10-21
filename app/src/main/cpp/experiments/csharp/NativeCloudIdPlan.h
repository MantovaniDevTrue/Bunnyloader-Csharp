#pragma once

#include "experiments/csharp/NativeCloudSelection.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace bl::csharp {

// A C# cloud cannot take CloudID.Count blindly: the original JS CloudLoader
// also allocates Count, Count+1, etc. The actual TextureAssets.Cloud AND
// TextureMaskManager.CloudMasks arrays must already reflect every JS mod.
// This pure policy is reused by native IL2CPP preflight and host tests.
struct NativeCloudIdPlan {
    int firstType = -1;
    int lastExclusive = -1;
    std::string error;
    bool valid() const { return error.empty() && firstType > 0; }
};

inline NativeCloudIdPlan planNativeCloudIds(
    int vanillaCloudCount, int textureArrayLength, int maskArrayLength,
    int cloudCount, bool jsCloudRegistrationSettled,
    bool mobileMasksLoaded) {
    auto deny = [](const char* reason) -> NativeCloudIdPlan {
        return {-1, -1, reason};
    };
    if (!jsCloudRegistrationSettled)
        return deny("CloudID: JS registrations not settled");
    if (!mobileMasksLoaded)
        return deny("CloudID: TextureMaskManager.LoadMasks not completed");
    if (vanillaCloudCount <= 0 || vanillaCloudCount > 10000 ||
        textureArrayLength < vanillaCloudCount ||
        maskArrayLength < vanillaCloudCount ||
        textureArrayLength > 10000 || maskArrayLength > 10000)
        return deny("CloudID: invalid or incomplete vanilla arrays");
    if (cloudCount <= 0 ||
        cloudCount > static_cast<int>(NativeCloudWeightedPool::kMaxModClouds))
        return deny("CloudID: cloud count out of bounds");

    // Reserve after both tables, never in a gap. Even previously removed JS
    // cloud entries cannot collide with our C# type IDs in this session.
    const int first = std::max({vanillaCloudCount, textureArrayLength,
                                maskArrayLength});
    if (first > 10000 ||
        cloudCount > 16384 - first)
        return deny("CloudID: insufficient type address space");
    return {first, first + cloudCount, ""};
}

} // namespace bl::csharp
