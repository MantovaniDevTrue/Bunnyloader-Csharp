#pragma once

#include "experiments/csharp/SourceLoader.h"
#include <string>

namespace bl::csharp {

inline int expectedManagedSourceTypes(const SourceModStatus& mod) {
    return mod.items + mod.tiles + mod.projectiles + mod.buffs +
           mod.players + mod.npcs + mod.prefixes + mod.screenEffects +
           mod.commands;
}

inline bool managedSourceCompileFinished(const std::string& status,
                                         const std::string& modId,
                                         const SourceModStatus& mod) {
    return status.find("state=finished\n") != std::string::npos &&
           status.find("modId=" + modId + "\n") != std::string::npos &&
           status.find("phase=compile-verified\n") != std::string::npos &&
           status.find("assemblyReady=1\n") != std::string::npos &&
           status.find("executedTypes=" + std::to_string(expectedManagedSourceTypes(mod)) +
                       "\n") != std::string::npos;
}

}
