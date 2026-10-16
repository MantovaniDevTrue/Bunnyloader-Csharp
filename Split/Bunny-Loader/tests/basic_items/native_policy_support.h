#pragma once
#include <map>
#include "experiments/csharp/ManagedVisualQueue.h"
#include "experiments/csharp/NativeContent.h"
namespace bl::testing {
extern std::string modContentRoot;
extern std::vector<csharp::ManagedVisualCommand> visualCommands;
extern std::map<Il2CppObject*,std::pair<int,int>> assetSizes;
extern Il2CppClass* assetClass;
extern Il2CppObject* (*loadAsset)(const std::string&);
extern std::vector<csharp::ManagedNativeEquipTexture> legacyEquips;
extern std::map<int,int> legacySlots;
}
