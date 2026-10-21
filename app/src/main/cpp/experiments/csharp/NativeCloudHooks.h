#pragma once

#include "experiments/csharp/NativeCloudRollCoordinator.h"
#include "experiments/csharp/NativeCloudSpawnLedger.h"

#include <map>
#include <string>
#include <vector>

namespace bl::csharp {

// Hooks and IL2CPP field bindings are staged, but no existing loader calls
// installNativeCloudHooks yet. The C# SourceLoader intentionally rejects all
// BunnyCloud classes until the complete MonoVM/game-thread bridge is tested.
bool installNativeCloudHooks(
    const NativeCloudRegistry* registry,
    const std::map<std::string, std::vector<int>>& managedCloudIds,
    bool monoWorkerConfirmed,
    std::string* error);

// Process Mono replies and issue new refresh requests on Terraria's game
// update thread; NEVER call from either Cloud hook or the sky renderer.
// Biome keys must be gathered from the actual player, not guessed.
void pumpNativeCloudManagedCallbacksOnGameThread(
    bool gameMenu, bool tenthAnniversaryWorld, const std::string& biomeKeys);

// Each weight snapshot is computed by MonoVM outside the frame loop and
// submitted on the game thread. A bad snapshot keeps the previous valid one.
bool publishNativeCloudWeights(const std::vector<NativeCloudWeight>& weights);
bool nativeCloudWeightsRefreshNeeded();

// The transport copies this plain-data event on the game thread and sends it
// to MonoVM; no Terraria object is allowed to cross that thread boundary.
bool takeNativeCloudSpawnOnGameThread(NativeCloudSpawnLedger::Event* event);

// Worker responses must be forwarded to the game thread, where the current
// Terraria cloud slot/type/generation are checked before a spriteDir write.
bool applyNativeCloudSpriteResultOnGameThread(
    const NativeCloudSpawnLedger::Event& event, int spriteDir);

// Never uninstall chained ShadowHook trampolines in world transitions.
// Pausing must precede any texture/mask cleanup.
void pauseNativeCloudHooks();

} // namespace bl::csharp
