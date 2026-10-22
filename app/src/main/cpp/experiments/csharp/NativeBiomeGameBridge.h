#pragma once

#include "experiments/csharp/NativeCloudBiomePolicy.h"

#include <string>

namespace bl::csharp {

// Staged only after the compiled Roslyn manifest and MonoVM class manifest
// agree. SourceLoader keeps BunnyBiome fail-closed until content installation.
bool stageNativeBiomeOwnerOnGameThread(const std::string& modId,
    const std::string& compiledManifest, std::string* error);

// No blocking calls to MonoVM. A new snapshot invalidates previous flags;
// only owned, current-generation responses may affect the cloud weight cache.
bool pumpNativeBiomeOwnersOnGameThread(const NativeCloudBiomeFacts& facts,
    bool newSnapshot, std::string* biomeKeys);

bool nativeBiomeOwnersStagedOnGameThread();
void resetNativeBiomeWorldStateOnGameThread();
void clearNativeBiomeOwnersOnGameThread();

} // namespace bl::csharp
