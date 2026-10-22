#pragma once

#include "experiments/csharp/NativeHairRegistry.h"
#include <functional>
#include <string>
#include <string_view>

namespace bl::csharp::hair {

struct TextureInstallResult {
    bool installed = false;
    int installedStyles = 0;
    std::string error;
};

// Somente na game thread IL2CPP, depois de TextureAssets inicializado.
// O retorno false mantém as duas tabelas originais; nunca altera assets vanilla.
TextureInstallResult InstallNativeHairTextures(
    const Registry& registry,
    const std::function<std::string(std::string_view)>& modContentDirectory);

}
