#pragma once

#include <string>
#include <map>
#include <vector>

namespace bl::mods { struct LoadedMod; }

namespace bl::csharp {

struct SourceModStatus {
    std::string id;
    std::string error;
    int sourceFiles = 0;
    int items = 0;
    int projectiles = 0;
    bool loaded = false;
    bool runtimeApplied = false;
    std::map<std::string, int> registeredTypes;
};

bool loadCSharpSourceMod(const bl::mods::LoadedMod& mod);
bool applyManagedSourceDefaults(const std::string& modId,
                                const std::string& runtimeValues, std::string* error);
std::vector<SourceModStatus> sourceModStatuses();

}
