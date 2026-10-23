#pragma once
#include <string>

namespace bl::csharp {
bool bindNativeAchievementAssembly(const std::string& mod, const std::string& assembly);
void retireNativeAchievementMod(const std::string& mod);
void clearNativeAchievementWorld();
void pumpNativeAchievementGameplayOnGameThread();
}
