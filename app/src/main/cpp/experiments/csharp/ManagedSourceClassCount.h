#pragma once

#include "experiments/csharp/SourceLoader.h"
#include <string>

namespace bl::csharp {

inline int expectedManagedSourceTypes(const SourceModStatus& mod) {
    return mod.trees + mod.gores + mod.mounts + mod.items + mod.tiles + mod.walls + mod.dusts +
           mod.projectiles + mod.buffs + mod.players + mod.npcs +
           mod.prefixes + mod.rarities + mod.localizationModules +
           mod.screenEffects + mod.commands + mod.worldSystems + mod.hookModules + mod.damageClasses;
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
