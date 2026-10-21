#pragma once
#include <string>

namespace bl::csharp {

// Registro da assembly validada, mas sem ativar conquistas até haver personagem.
bool queueAchievementProfileMod(const std::string& modId,
                                const std::string& assembly,
                                const std::string& persistentRoot);

void pumpAchievementProfileOnGameThread(bool inWorld);
bool achievementProfileReady();
}
