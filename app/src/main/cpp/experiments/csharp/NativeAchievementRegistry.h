#pragma once

#include "experiments/csharp/ManagedAchievementMetadata.h"
#include "il2cpp/Types.h"
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

// Single-game-thread replacement for a different character. The adapter
// verifies and removes only the exact C# objects from AchievementManager.
// Refuses mutation when the Terraria runtime dictionary ABI differs.
bool replaceNativeAchievementObjects(
    const std::vector<AchievementMetadata>& definitions,
    std::string* error);

// Retorna o mesmo Asset<Texture2D> nativo que o Bunny usa no menu JS.
// Chamado apenas na thread Terraria; ponteiro permanece enraizado até unload.
Il2CppObject* nativeAchievementIconAsset(const Il2CppObject* achievement);

// After the achievement menu has rebuilt its native cache, old GC roots may be released.
// Safely detach only this mod's exact native Achievement objects. Other
// mods and vanilla achievements are not touched. Failure attempts rollback.
bool unregisterNativeAchievementMod(const std::string& modId, std::string* error);
bool nativeAchievementModRegistered(const std::string& modId);

void releaseRetiredNativeAchievements();

NativeAchievementRegistryStatus nativeAchievementRegistryStatus();

}
