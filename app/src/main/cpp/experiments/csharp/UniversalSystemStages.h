#pragma once
#include <string>
namespace bl::csharp {
inline constexpr int UniversalSystemStageCount=14;
inline constexpr bool isPublicUniversalSystemStage(int index) {return index>=0 && index<UniversalSystemStageCount && index!=11;}
int installUniversalSystemStageOnGameThread(const std::string& name, int slot);
void removeUniversalSystemStage(int handle);
void disableAllUniversalSystemStages();
void pauseWorldUniversalSystemStages();
inline constexpr bool isSessionBackgroundStage(int index){return index==13;}
bool isUniversalHookGameThread();
}
