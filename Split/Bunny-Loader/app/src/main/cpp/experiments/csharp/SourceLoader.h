#pragma once

#include <string>
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
};

bool loadCSharpSourceMod(const bl::mods::LoadedMod& mod);
std::vector<SourceModStatus> sourceModStatuses();

}
