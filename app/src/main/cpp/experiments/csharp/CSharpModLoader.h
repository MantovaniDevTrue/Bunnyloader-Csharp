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
    bool npcBehavior = false;
    int npcBindings = 0;
    bool npcCombat = false;
    bool nativeContent = false;
    int nativeItems = 0;
    int nativeProjectiles = 0;
    int nativeNpcs = 0;
    int nativeAccessories = 0;
    int nativeBuffs = 0;
    int nativePotions = 0;
    int nativeFirstNpcType = -1;
    int nativeFirstAccessoryType = -1;
    int nativeFirstBuffType = -1;
};

void loadAll();
size_t loadedCount();
std::vector<LoadedNativeMod> snapshot();

bool hasProjectileBehavior();
bool hasNpcBehavior();
bool dispatchOwnedNpcCombat(int npcType, bl::csharp::HostNpcState* state,
                            float playerX, float playerY, bl::csharp::HostNpcAttack* attack);
bool hasOwnedNpcBehavior(int npcType);
bool dispatchOwnedNpc(int npcType, bl::csharp::HostNpcState* state, float playerX, float playerY);
void releaseOwnedNpc(int npcType, int slot, int generation);
bool hasOwnedProjectileBehavior(int projectileType);
void releaseOwnedProjectile(int projectileType, int slot, int owner, int generation);
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
