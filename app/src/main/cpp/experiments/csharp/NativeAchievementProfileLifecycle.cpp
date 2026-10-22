#include "experiments/csharp/NativeAchievementProfileLifecycle.h"

#include "experiments/csharp/NativeAchievementProfile.h"
#include "experiments/csharp/NativeAchievementRegistry.h"
#include "experiments/csharp/NativeAchievementHooks.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "core/Log.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

struct ModInfo {
    std::string id;
    std::string assembly;
    bool enabled = true;
};
struct ManifestInfo {
    std::string id;
    std::string assembly;
    std::string manifest;
};
enum class Phase {
    Pending, Loading, Completed, Ready, Switching, SwitchCompleted,
    ModUnloading, ModUnloadReady, ModUnloadFinishing, ModUnloadFinalized,
    ModReloading, ModReloadReady, ModReloadApplying, Blocked
};
std::mutex mutex;
std::vector<ModInfo> mods;
std::vector<ManifestInfo> loaded;
std::string persistentRoot;
std::string selectedProfile;
std::string targetProfile;
std::string loadError;
std::string lifecycleMod;
std::string lifecycleManifest;
Phase phase = Phase::Pending;
std::atomic<bool> active{false};
unsigned int attempts = 0;
bool reportedMismatch = false;

bool ensureDirectory(const std::string& directory) {
    if (directory.empty() || directory.size() > 2048) return false;
    if (mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST)
        return false;
    struct stat info{};
    return stat(directory.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

void loadProfile(std::string directory, std::vector<ModInfo> localMods) {
    std::vector<ManifestInfo> manifests;
    std::string error;
    if (!ensureDirectory(directory)) {
        error = "Profile save directory unavailable";
    } else {
        for (const auto& item : localMods) {
            std::string response;
            if (!invokeManagedAchievementControl(
                    item.id, item.assembly, 9, directory, &response, 15000) ||
                response != "READY") {
                error = "ConfigureStorage failed mod=" + item.id + " " +
                        response.substr(0, 120);
                break;
            }
            if (!invokeManagedAchievementControl(
                    item.id, item.assembly, 8, "", &response, 15000) ||
                !response.starts_with("BACH_META_V2\n")) {
                error = "Profile metadata failed mod=" + item.id + " " +
                        response.substr(0, 120);
                break;
            }
            manifests.push_back({item.id, item.assembly, std::move(response)});
        }
    }
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::Loading) return;
    loaded = std::move(manifests);
    loadError = std::move(error);
    phase = Phase::Completed;
}

void switchProfile(std::string directory, std::string previousDirectory,
                   std::vector<ModInfo> localMods) {
    std::vector<ManifestInfo> manifests;
    std::vector<ModInfo> changed;
    std::string error;
    if (!ensureDirectory(directory)) error = "New character save directory unavailable";
    for (const auto& item : localMods) {
        if (!error.empty()) break;
        std::string response;
        if (!invokeManagedAchievementControl(item.id, item.assembly, 11,
                 directory, &response, 15000) ||
            !response.starts_with("BACH_META_V2\n")) {
            error = "SwitchProfile failed mod=" + item.id + " " +
                    response.substr(0, 120);
            break;
        }
        changed.push_back(item);
        manifests.push_back({item.id, item.assembly, std::move(response)});
    }
    if (!error.empty()) {
        // No game thread is permitted to resume gameplay. First reverse
        // any successful managed switches while preserving original saves.
        for (auto it = changed.rbegin(); it != changed.rend(); ++it) {
            std::string result;
            if (!invokeManagedAchievementControl(
                    it->id, it->assembly, 11, previousDirectory,
                    &result, 15000) ||
                !result.starts_with("BACH_META_V2\n"))
                error += " / MANAGED_ROLLBACK_UNKNOWN mod=" + it->id;
        }
    }
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::Switching) return;
    loaded = std::move(manifests);
    loadError = std::move(error);
    phase = Phase::SwitchCompleted;
}

void prepareModUnload(ModInfo target) {
    std::string result;
    std::string error;
    if (!invokeManagedAchievementControl(
            target.id, target.assembly, 10, "", &result, 15000) ||
        result != "OK")
        error = "Final achievement save failed: " + result.substr(0, 120);
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::ModUnloading || lifecycleMod != target.id) return;
    loadError = std::move(error);
    phase = Phase::ModUnloadReady;
}

void finishModUnload(ModInfo target) {
    std::string result;
    std::string error;
    if (!invokeManagedAchievementControl(
            target.id, target.assembly, 7, "", &result, 15000) ||
        !result.starts_with("BACH1\n"))
        error = "Managed achievement unload failed: " + result.substr(0, 120);
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::ModUnloadFinishing || lifecycleMod != target.id) return;
    loadError = std::move(error);
    phase = Phase::ModUnloadFinalized;
}

void prepareModReload(ModInfo target, std::string directory) {
    std::string result;
    std::string error;
    if (!invokeManagedAchievementControl(
            target.id, target.assembly, 0, "", &result, 15000) ||
        !result.starts_with("BACH_META_V2\n")) {
        error = "Managed achievement rebind failed: " + result.substr(0, 120);
    } else if (!invokeManagedAchievementControl(
            target.id, target.assembly, 9, directory, &result, 15000) ||
        result != "READY") {
        error = "Managed profile reload failed: " + result.substr(0, 120);
    } else if (!invokeManagedAchievementControl(
            target.id, target.assembly, 8, "", &result, 15000) ||
        !result.starts_with("BACH_META_V2\n")) {
        error = "Reloaded achievement metadata invalid: " +
                result.substr(0, 120);
    }
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::ModReloading || lifecycleMod != target.id) return;
    lifecycleManifest = error.empty() ? std::move(result) : "";
    loadError = std::move(error);
    phase = Phase::ModReloadReady;
}

}

bool queueAchievementProfileMod(const std::string& modId,
                                const std::string& assembly,
                                const std::string& root) {
    if (modId.empty() || assembly.empty() || root.empty() ||
        root.front() != '/' || root.size() > 1900)
        return false;
    std::lock_guard<std::mutex> guard(mutex);
    if (phase != Phase::Pending) return false;
    if (!persistentRoot.empty() && persistentRoot != root) return false;
    for (const auto& mod : mods)
        if (mod.id == modId) return mod.assembly == assembly;
    if (mods.size() >= 64) return false;
    persistentRoot = root;
    mods.push_back({modId, assembly});
    return true;
}

bool requestAchievementModUnload(const std::string& modId,
                                 std::string* error) {
    if (!error) return false;
    error->clear();
    if (!nativeAchievementModRegistered(modId)) {
        *error = "Cannot unload mod until native achievement registration completes";
        return false;
    }
    ModInfo selected;
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (phase != Phase::Ready || !active.load(std::memory_order_acquire)) {
            *error = "Achievement profile is not ready for mod unload";
            return false;
        }
        auto found = std::find_if(mods.begin(), mods.end(),
            [&](const ModInfo& mod) { return mod.id == modId && mod.enabled; });
        if (found == mods.end()) {
            *error = "Active achievement mod not found";
            return false;
        }
        selected = *found;
        lifecycleMod = modId;
        loadError.clear();
        phase = Phase::ModUnloading;
        active.store(false, std::memory_order_release);
    }
    setNativeAchievementWorldActive(false);
    std::thread([selected = std::move(selected)]() {
        prepareModUnload(selected);
    }).detach();
    return true;
}

bool requestAchievementModReload(const std::string& modId,
                                 std::string* error) {
    if (!error) return false;
    error->clear();
    ModInfo selected;
    std::string directory;
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (phase != Phase::Ready || selectedProfile.empty()) {
            *error = "Achievement profile is not ready for mod reload";
            return false;
        }
        auto found = std::find_if(mods.begin(), mods.end(),
            [&](const ModInfo& mod) { return mod.id == modId && !mod.enabled; });
        if (found == mods.end()) {
            *error = "Disabled achievement mod not found";
            return false;
        }
        selected = *found;
        directory = persistentRoot + "/profiles/" + selectedProfile;
        lifecycleMod = modId;
        lifecycleManifest.clear();
        loadError.clear();
        phase = Phase::ModReloading;
        active.store(false, std::memory_order_release);
    }
    setNativeAchievementWorldActive(false);
    std::thread([selected = std::move(selected), directory = std::move(directory)]() {
        prepareModReload(selected, directory);
    }).detach();
    return true;
}

bool achievementProfileReady() {
    return active.load(std::memory_order_acquire);
}

void pauseAchievementProfileOnWorldExit() {
    active.store(false, std::memory_order_release);
    setNativeAchievementWorldActive(false);
}

void pumpAchievementProfileOnGameThread(bool inWorld) {
    if (!inWorld) return;
    if (++attempts % (achievementProfileReady() ? 60u : 12u) != 1u)
        return;

    std::string key, error;
    if (!activeAchievementProfile(&key, &error)) {
        if (achievementProfileReady()) {
            active.store(false, std::memory_order_release);
            setNativeAchievementWorldActive(false);
            if (!reportedMismatch) {
                BL_WARN("[CSharp][AchievementLoader] perfil indisponivel; eventos pausados");
                reportedMismatch = true;
            }
        }
        return;
    }

    std::vector<ModInfo> toLoad;
    std::string directory;
    bool finishUnload = false;
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (mods.empty() || phase == Phase::Blocked) return;
        if (!selectedProfile.empty() && selectedProfile != key &&
            phase != Phase::Ready && phase != Phase::Switching &&
            phase != Phase::SwitchCompleted) {
            phase = Phase::Blocked;
            active.store(false, std::memory_order_release);
            setNativeAchievementWorldActive(false);
            BL_WARN("[CSharp][AchievementLoader] personagem mudou durante ciclo do mod");
            return;
        }

        if (phase == Phase::Pending) {
            if (!ensureDirectory(persistentRoot) ||
                !ensureDirectory(persistentRoot + "/profiles")) {
                loadError = "Cannot create profile storage root";
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] %s", loadError.c_str());
                return;
            }
            selectedProfile = key;
            directory = persistentRoot + "/profiles/" + selectedProfile;
            toLoad = mods;
            phase = Phase::Loading;
        } else if (phase == Phase::Completed) {
            if (!loadError.empty()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] Perfil recusado: %s", loadError.c_str());
                return;
            }
            for (const auto& item : loaded) {
                std::string bindError;
                if (!registerNativeAchievementMod(
                        item.id, item.assembly, item.manifest, &bindError)) {
                    phase = Phase::Blocked;
                    BL_WARN("[CSharp][AchievementLoader] Bind nativo recusado mod=%s: %s",
                            item.id.c_str(), bindError.c_str());
                    return;
                }
            }
            loaded.clear();
            phase = Phase::Ready;
            active.store(true, std::memory_order_release);
            BL_INFO("[CSharp][AchievementLoader] Perfil isolado restaurado; mods=%zu",
                    mods.size());
            return;
        } else if (phase == Phase::ModUnloadReady) {
            if (!loadError.empty()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] save final recusado: %s",
                        loadError.c_str());
                return;
            }
            if (!prepareNativeAchievementBindingUnload(lifecycleMod, &error)) {
                // Retentar no próximo ciclo sem bloquear a thread GL.
                return;
            }
            if (!unregisterNativeAchievementMod(lifecycleMod, &error) ||
                !unregisterNativeAchievementBinding(lifecycleMod, &error)) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] unload nativo bloqueado: %s",
                        error.c_str());
                return;
            }
            auto found = std::find_if(mods.begin(), mods.end(),
                [&](const ModInfo& mod) { return mod.id == lifecycleMod; });
            if (found == mods.end()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] unload sem mod registrado");
                return;
            }
            toLoad.push_back(*found);
            phase = Phase::ModUnloadFinishing;
            finishUnload = true;
        } else if (phase == Phase::ModUnloadFinalized) {
            if (!loadError.empty()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] unload MonoVM recusado: %s",
                        loadError.c_str());
                return;
            }
            auto found = std::find_if(mods.begin(), mods.end(),
                [&](const ModInfo& mod) { return mod.id == lifecycleMod; });
            if (found != mods.end()) found->enabled = false;
            lifecycleMod.clear();
            phase = Phase::Ready;
            const bool anyEnabled = std::any_of(mods.begin(), mods.end(),
                [](const ModInfo& mod) { return mod.enabled; });
            active.store(anyEnabled, std::memory_order_release);
            BL_INFO("[CSharp][AchievementLoader] mod descarregado; restantes=%d",
                    anyEnabled ? 1 : 0);
            return;
        } else if (phase == Phase::ModReloadReady) {
            if (!loadError.empty()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] reload MonoVM recusado: %s",
                        loadError.c_str());
                return;
            }
            auto found = std::find_if(mods.begin(), mods.end(),
                [&](const ModInfo& mod) { return mod.id == lifecycleMod; });
            if (found == mods.end()) {
                phase = Phase::Blocked;
                return;
            }
            std::vector<AchievementMetadata> verified;
            if (!parseAchievementMetadata(lifecycleManifest, found->id,
                                          &verified, &error) ||
                !registerNativeAchievementMod(
                    found->id, found->assembly, lifecycleManifest, &error)) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] reload metadata recusado: %s",
                        error.c_str());
                return;
            }
            phase = Phase::ModReloadApplying;
            return;
        } else if (phase == Phase::ModReloadApplying) {
            if (!nativeAchievementModRegistered(lifecycleMod)) return;
            auto found = std::find_if(mods.begin(), mods.end(),
                [&](const ModInfo& mod) { return mod.id == lifecycleMod; });
            if (found == mods.end()) {
                phase = Phase::Blocked;
                return;
            }
            found->enabled = true;
            lifecycleMod.clear();
            lifecycleManifest.clear();
            phase = Phase::Ready;
            active.store(true, std::memory_order_release);
            BL_INFO("[CSharp][AchievementLoader] mod recarregado e ativado");
            return;
        } else if (phase == Phase::Ready && selectedProfile != key) {
            active.store(false, std::memory_order_release);
            setNativeAchievementWorldActive(false);
            targetProfile = key;
            directory = persistentRoot + "/profiles/" + targetProfile;
            for (const auto& mod : mods)
                if (mod.enabled) toLoad.push_back(mod);
            phase = Phase::Switching;
            BL_INFO("[CSharp][AchievementLoader] preparando troca de personagem");
        } else if (phase == Phase::SwitchCompleted) {
            if (key != targetProfile || !loadError.empty()) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] troca de perfil bloqueada: %s",
                        loadError.empty() ? "character changed mid-switch" :
                                            loadError.c_str());
                return;
            }
            std::vector<AchievementMetadata> definitions;
            for (const auto& item : loaded) {
                std::vector<AchievementMetadata> current;
                if (!parseAchievementMetadata(item.manifest, item.id,
                                              &current, &error)) {
                    phase = Phase::Blocked;
                    BL_WARN("[CSharp][AchievementLoader] profile metadata invalid: %s",
                            error.c_str());
                    return;
                }
                definitions.insert(definitions.end(), current.begin(), current.end());
            }
            if (!replaceNativeAchievementObjects(definitions, &error)) {
                phase = Phase::Blocked;
                BL_WARN("[CSharp][AchievementLoader] native character swap rejected: %s",
                        error.c_str());
                return;
            }
            for (const auto& item : loaded) {
                std::string bindError;
                if (!registerNativeAchievementMod(
                        item.id, item.assembly, item.manifest, &bindError)) {
                    phase = Phase::Blocked;
                    BL_WARN("[CSharp][AchievementLoader] profile rebind failed mod=%s: %s",
                            item.id.c_str(), bindError.c_str());
                    return;
                }
            }
            selectedProfile = targetProfile;
            targetProfile.clear();
            loaded.clear();
            phase = Phase::Ready;
            active.store(true, std::memory_order_release);
            BL_INFO("[CSharp][AchievementLoader] personagem trocado com rollback nativo disponivel");
            return;
        } else if (phase == Phase::Ready) {
            // A flag pode ter sido suspensa se o getter ficou indisponível.
            active.store(true, std::memory_order_release);
            reportedMismatch = false;
            return;
        }
    }
    if (finishUnload && toLoad.size() == 1) {
        std::thread([target = std::move(toLoad.front())]() {
            finishModUnload(target);
        }).detach();
        return;
    }
    if (!directory.empty()) {
        const bool isSwitch = !targetProfile.empty();
        const std::string original = persistentRoot + "/profiles/" + selectedProfile;
        std::thread([directory = std::move(directory),
                     original, mods = std::move(toLoad), isSwitch]() mutable {
            if (isSwitch)
                switchProfile(std::move(directory), original, std::move(mods));
            else loadProfile(std::move(directory), std::move(mods));
        }).detach();
    }
}

}
