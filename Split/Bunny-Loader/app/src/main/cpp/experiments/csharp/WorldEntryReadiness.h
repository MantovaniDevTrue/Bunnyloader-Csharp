#pragma once
namespace bl::csharp {
// Texture/equipment assets already have the deferred path. Never bypass core content.
inline bool worldDependentAssetsMayDefer(int pendingCore, int pendingRecipes,
                                         int pendingPlacements, bool registered,
                                         bool bindingsReady, bool itemsReady) {
    return pendingCore == 0 && pendingRecipes == 0 && pendingPlacements == 0 &&
           registered && bindingsReady && itemsReady;
}
}
