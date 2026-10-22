#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <string>

namespace bl::csharp::hair {

struct HairPlayerIoStatus {
    bool hooked = false;
    bool enabled = false;
    std::string error;
};

// Somente na game thread depois que o Registry e as texturas estiverem prontos.
// Instala hooks em Player.InternalSavePlayerFile e Player.LoadPlayer.
// Desligado até SourceLoader/AutoSourceRuntime terminar todos os contratos.
HairPlayerIoStatus InstallNativeHairPlayerIo(const Registry* registry,
                                             bool texturesInstalled);

void ResetNativeHairPlayerIo();

} // namespace bl::csharp::hair
