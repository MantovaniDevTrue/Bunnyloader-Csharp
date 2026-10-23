#pragma once
#include <string>
#include "experiments/csharp/NativeAchievementState.h"

namespace bl::csharp {
bool bindNativeAchievementAssembly(const std::string& mod, const std::string& assembly);
void retireNativeAchievementMod(const std::string& mod);
void clearNativeAchievementWorld();
void setNativeAchievementWorldState(bool ready);
void pumpNativeAchievementGameplayOnGameThread();
bool getNativeAchievementState(const std::string& mod, const std::string& name,
                               achievement::AchievementState* output);
}
