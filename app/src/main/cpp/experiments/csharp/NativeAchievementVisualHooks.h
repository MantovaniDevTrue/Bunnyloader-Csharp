#pragma once
#include <string>

namespace bl::csharp {

// Chamado somente na thread principal do jogo e depois do registro nativo.
// Confere classes, tipos e overloads antes de tocar na UI mobile.
bool installNativeAchievementVisualHooks(std::string* error);
bool nativeAchievementVisualHooksReady();
}
