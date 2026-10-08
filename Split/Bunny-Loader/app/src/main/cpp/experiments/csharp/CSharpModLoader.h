#pragma once

#include "experiments/csharp/CSharpHostTypes.h"

#include <string>
#include <vector>

namespace bl::csharpmods {

struct LoadedNativeMod {
    std::string id;
    std::string sourcePath;
    std::string loadedPath;
    int abiVersion = 0;
    int hostApiVersion = 0;
    bool loaded = false;
    bool projectileBehavior = false;
    bool selectiveProjectiles = false;
    int projectileBindings = 0;
};

void loadAll();
size_t loadedCount();
std::vector<LoadedNativeMod> snapshot();

bool hasProjectileBehavior();
bool hasOwnedProjectileBehavior(int projectileType);
bool dispatchOwnedProjectile(int projectileType,
                             bl::csharp::HostProjectileState* state,
                             const bl::csharp::NpcSample* npcs,
                             int count,
                             float playerX,
                             float playerY);
bool dispatchProjectile(bl::csharp::HostProjectileState* state,
                        const bl::csharp::NpcSample* npcs,
                        int count,
                        float playerX,
                        float playerY);

}
