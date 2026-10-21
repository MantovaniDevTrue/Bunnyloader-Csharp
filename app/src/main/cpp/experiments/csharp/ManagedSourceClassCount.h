#pragma once

#include "experiments/csharp/SourceLoader.h"
#include <string>

namespace bl::csharp {

inline int expectedManagedSourceTypes(const SourceModStatus& mod) {
    return mod.items + mod.tiles + mod.walls + mod.dusts +
           mod.projectiles + mod.buffs + mod.players + mod.npcs +
           mod.prefixes + mod.rarities + mod.localizationModules +
           mod.screenEffects + mod.commands + mod.backgroundTextures;
}

// Module inventory comes from the verified assembly, not a source substring.
// Native content still requires exact identity/cardinality validation in V2.
inline int verifiedHookModuleCount(const std::string& status) {
    const std::string marker = "\nhookModules=";
    const auto at = status.find(marker);
    if (at == std::string::npos) return 0; // old compiler statuses
    const auto first = at + marker.size(), last = status.find('\n', first);
    if (last == std::string::npos || last == first || last - first > 4 || status.find(marker, last) != std::string::npos) return -1;
    int count = 0;
    for (size_t i = first; i < last; ++i) {
        if (status[i] < '0' || status[i] > '9') return -1;
        count = count * 10 + status[i] - '0';
    }
    return count <= 4096 ? count : -1;
}

inline bool managedSourceCompileFinished(const std::string& status,
                                         const std::string& modId,
                                         const SourceModStatus& mod) {
    const int modules = verifiedHookModuleCount(status);
    if (modules < 0 || expectedManagedSourceTypes(mod) + modules <= 0) return false;
    return status.find("state=finished\n") != std::string::npos &&
           status.find("modId=" + modId + "\n") != std::string::npos &&
           status.find("phase=compile-verified\n") != std::string::npos &&
           status.find("assemblyReady=1\n") != std::string::npos &&
           status.find("executedTypes=" + std::to_string(expectedManagedSourceTypes(mod) + modules) +
                       "\n") != std::string::npos;
}

}
