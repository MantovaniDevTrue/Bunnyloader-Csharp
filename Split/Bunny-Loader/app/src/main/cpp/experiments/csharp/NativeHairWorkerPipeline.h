#pragma once

#include "experiments/csharp/NativeHairStartup.h"

#include <functional>
#include <string>
#include <string_view>

namespace bl::csharp::hair {

struct WorkerHairResult {
    int count = 0;
    int firstType = -1;
    bool ready = false;
    std::string phase;
    std::string error;
};

using ManagedHairExporter = std::function<int(int firstReservedType)>;

// Opera no worker, enquanto a caixa de mensagens atende snapshots na game
// thread. Não toca em TextureAssets ou instala hooks. Se qualquer etapa
// falhar, invalida todas as reservas desta sessão em vez de publicar metade.
WorkerHairResult PrepareOneCompiledHairMod(
    NativeHairStartup* startup, std::string_view uid,
    int verifiedHairClassCount, std::string_view privateWorkDir,
    const ManagedHairExporter& exportManifest,
    const Registry::AssetExists& assetExists);

// Produção: usa o MonoVM real para contar e exportar classes, e o reader
// nativo seguro para importar os bytes. SourceLoader deve continuar
// bloqueando o uso até a instalação UI/save/rede/unload completa.
WorkerHairResult PrepareCompiledHairModOnWorker(
    NativeHairStartup* startup,
    const std::string& probeAssembly, const std::string& compilerAssembly,
    const std::string& compiledModAssembly, const std::string& modUid,
    const std::string& modRoot, const std::string& privateWorkDir,
    const Registry::AssetExists& assetExists);

} // namespace bl::csharp::hair
