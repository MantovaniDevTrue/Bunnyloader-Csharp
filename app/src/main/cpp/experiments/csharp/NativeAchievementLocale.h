#pragma once

#include "experiments/csharp/ManagedAchievementMetadata.h"

#include <string>
#include <vector>

namespace bl::csharp {

// Consultar somente na thread Terraria; a cultura vem de
// Terraria.Localization.Language.ActiveCulture.Name.
bool currentNativeAchievementCulture(std::string* culture, std::string* error);

// Atualizar os LocalizedText enraizados e preservados pelo registro.
// Chamado apenas ao mudar a cultura; jamais por item ou quadro.
bool updateNativeAchievementTranslations(
    const std::vector<AchievementMetadata>& definitions,
    const std::string& culture, std::string* error);

}
