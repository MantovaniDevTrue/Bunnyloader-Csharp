#pragma once
#include <string>

namespace bl::csharp {

// Registro da assembly validada, mas sem ativar conquistas até haver personagem.
bool queueAchievementProfileMod(const std::string& modId,
                                const std::string& assembly,
                                const std::string& persistentRoot);

void pumpAchievementProfileOnGameThread(bool inWorld);
bool achievementProfileReady();
void pauseAchievementProfileOnWorldExit();

// Explicit host lifecycle entrypoints. Called on the Terraria game thread;
// these schedule ordered MonoVM worker RPCs without blocking rendering.
bool requestAchievementModUnload(const std::string& modId, std::string* error);
bool requestAchievementModReload(const std::string& modId, std::string* error);
}
