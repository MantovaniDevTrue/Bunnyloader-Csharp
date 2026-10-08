#pragma once

#include "experiments/csharp/CSharpHostTypes.h"
#include <vector>

namespace bl::csharp {

struct TerrariaSnapshot {
    bool ready = false;
    PlayerSample player{};
    std::vector<NpcSample> npcs;
    std::vector<ProjectileSample> projectiles;
    int activeNpcs = 0;
    int activeProjectiles = 0;
    double captureMs = 0.0;
};

TerrariaSnapshot captureTerrariaSnapshot(bool includeProjectiles = true, bool includeNpcs = true);

}
