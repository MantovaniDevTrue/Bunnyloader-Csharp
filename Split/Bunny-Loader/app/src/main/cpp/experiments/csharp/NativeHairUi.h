#pragma once

#include "experiments/csharp/NativeHairUnlockState.h"

#include <string>

namespace bl::csharp::hair {

// Instalar somente na thread IL2CPP, depois das duas TextureAssets.
struct HairUiStatus {
    bool hooksInstalled = false;
    bool contentEnabled = false;
    int visibleStyles = 0;
    std::string error;
};

// O callback consulta GetUnlockConditions do Mono apenas no momento em que
// a lista precisa ser reavaliada. Se o callback estiver indisponível,
// cabelos de mod não aparecem no Stylist.
HairUiStatus InstallNativeHairUi(const Registry* registry, UnlockQuery query,
                                 void* context, bool texturesInstalled);

// Desativa imediatamente qualquer callback ou registro antigo durante unload.
// Os hooks nativos permanecem instalados, mas chamam o método vanilla.
void ResetNativeHairUi();

} // namespace bl::csharp::hair
