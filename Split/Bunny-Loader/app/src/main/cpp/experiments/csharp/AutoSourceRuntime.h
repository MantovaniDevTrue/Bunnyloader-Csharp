#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace bl::csharp {

struct AutoSourceStatus {
    std::string modId;
    std::string phase;
    std::string error;
    bool cached = false;
    bool applied = false;
    bool managedReady = false;
    int curves = 0;
    int enabledCurves = 0;
    int hitPolicies = 0;
    int killPolicies = 0;
    int enabledHits = 0;
    int enabledKills = 0;
    int netMode = -1;
    int gameMenu = -1;
    bool worldReady = false;
};

struct AutoWorldLifecycleStatus {
    uint64_t epoch = 0;
    int worldLoads = 0;
    int worldExits = 0;
    int rebindSchedules = 0;
    int configuredBindings = 0;
    bool worldReady = false;
};

AutoWorldLifecycleStatus autoWorldLifecycleStatus();
void startAutoSourceRuntime();
bool preWorldCSharpContentReady();
bool installCSharpWorldEntryGate();
void noteAutoWorldTilesLoaded();
void noteAutoGameUpdate();
bool submitInteractiveConsoleSource(const std::string& id, const std::string& root,
                                    const std::string& workDir, std::string* error);
std::vector<std::string> takeInteractiveConsoleOutput();
std::vector<AutoSourceStatus> autoSourceStatuses();

}
