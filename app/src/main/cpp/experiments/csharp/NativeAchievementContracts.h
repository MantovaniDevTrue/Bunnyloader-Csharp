#pragma once

#include "experiments/csharp/NativeAchievementManifest.h"

#include <string>
#include <vector>

namespace bl::csharp::achievement {

struct ManagerContractResult {
    bool available = false;
    bool canConstruct = false;
    bool canRegister = false;
    bool canReload = false;
    bool canPresent = false;
    std::string diagnostic;
};

ManagerContractResult inspectManagerContracts(const std::vector<Descriptor>& descriptors);

}
