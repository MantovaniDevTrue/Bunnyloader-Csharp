#pragma once

#include "experiments/csharp/ManagedAchievementMetadata.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp {

struct NativeAchievementRegistryStatus {
    std::size_t registered = 0;
    std::size_t pending = 0;
    std::uint64_t appliedRevisions = 0;
    bool bindingsReady = false;
    std::string error;
};

// Deve executar exclusivamente na thread IL2CPP do Terraria, apos inicializacao
// do Main.Achievements. Nenhum objeto nativo atravessa o worker MonoVM.
bool registerNativeAchievementObjects(
    const std::vector<AchievementMetadata>& definitions,
    std::string* error);

// Aplica metadata ja consolidada pelo worker; evita polling por quadro.
bool reconcileNativeAchievementObjects(
    const std::vector<AchievementMetadata>& definitions,
    std::string* error);

NativeAchievementRegistryStatus nativeAchievementRegistryStatus();

}
