#pragma once

#include <string>
#include <vector>

namespace bl::csharp {

struct AutoSourceStatus {
    std::string modId;
    std::string phase;
    std::string error;
    bool cached = false;
    bool applied = false;
    int curves = 0;
    int enabledCurves = 0;
    int netMode = -1;
    int gameMenu = -1;
};

void startAutoSourceRuntime();
std::vector<AutoSourceStatus> autoSourceStatuses();

}
