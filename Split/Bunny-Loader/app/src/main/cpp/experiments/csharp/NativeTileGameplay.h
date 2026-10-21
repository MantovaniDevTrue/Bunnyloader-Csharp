#pragma once
#include <string>
#include <vector>
#include "experiments/csharp/NativeTileDropPolicy.h"

namespace bl::csharp {

struct NativeTileGameplaySpec {
    std::string mod;
    std::string name;
    std::string dustName;
    int type = -1;
    int shape = 0;
    int failDust = 1;
    int normalDust = 3;
    int animationFrameHeight = 0;
    int animationFrames = 1;
    int animationFrameTicks = 0;
    bool allowBreaking = true;
    bool allowDrops = true;
    int callbackMask = 0;
    bool cacheDrawData = true;
    std::string assembly;
    int frameToggle=0;
    bool campfire=false;
    std::vector<tile_drop::Rule> registeredDrops;
    int minPick = 0;
    float mineResist = 1.f;
    int saplingGroundType = -1;
    int saplingVanillaGroundType = -1;
    int saplingGrowthChance = 0;
};

bool registerNativeTileGameplay(const NativeTileGameplaySpec& spec);
void bindNativeTileAssembly(const std::string& mod,const std::string& assembly);
void resetNativeTileSessionState();

}
