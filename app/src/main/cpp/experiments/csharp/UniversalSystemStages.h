#pragma once
#include <string>
namespace bl::csharp {
int installUniversalSystemStageOnGameThread(const std::string& name, int slot);
void removeUniversalSystemStage(int handle);
void disableAllUniversalSystemStages();
bool isUniversalHookGameThread();
}
