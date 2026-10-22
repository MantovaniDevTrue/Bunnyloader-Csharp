#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "experiments/csharp/ManagedAchievementMetadata.h"

namespace bl::csharp {

// Chamado após BunnyAchievementRuntime.Bind confirmar os metadados.
// Não registra tipos de item e não instala hooks no worker de compilação.
bool registerNativeAchievementMod(const std::string& modId,
                                  const std::string& assemblyPath,
                                  const std::string& metadata,
                                  std::string* error);
std::vector<AchievementMetadata> nativeAchievementMetadataSnapshot();
std::uint64_t nativeAchievementMetadataRevision();
// Chamado no worker após um evento; atualiza apenas estado mutável da UI.
bool updateNativeAchievementProgress(const std::string& modId,
                                     const std::string& snapshot,
                                     std::string* error);

bool nativeAchievementBindingsReady();
bool nativeAchievementHooksReady();

// Deve ser chamado apenas na thread do Terraria, depois de resolver GameRefs.
// Falha fechada se qualquer uma das quatro assinaturas IL2CPP divergir.
bool installNativeAchievementHooks(std::string* error);

// Os ganchos permanecem instalados, mas entregam eventos somente dentro do
// mundo do jogador local. A transição entre mundos altera apenas essa flag.
void setNativeAchievementWorldActive(bool active);
void flushNativeAchievementProgressOnExit();
void pumpNativeAchievementPersistence(bool inWorld);

}
