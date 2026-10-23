#pragma once

#include <vector>
#include "experiments/csharp/NativeAnglerQuestRegistry.h"

namespace bl::csharp::angler {

bool installFishingOnGameThread(const std::vector<Quest>* quests);
bool fishingInstalled();
const char* fishingInstallDiagnostic();

}
