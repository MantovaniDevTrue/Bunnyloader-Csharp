#include "experiments/csharp/NativeAchievementProfileLifecycle.h"

#include "experiments/csharp/NativeAchievementProfile.h"
#include "experiments/csharp/NativeAchievementRegistry.h"
#include "experiments/csharp/NativeAchievementHooks.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "core/Log.h"

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
};
struct ManifestInfo {
    std::string id;
    std::string assembly;
    std::string manifest;
};
enum class Phase { Pending, Loading, Completed, Ready, Switching, SwitchCompleted, Blocked };
std::mutex mutex;
std::vector<ModInfo> mods;
std::vector<ManifestInfo> loaded;
std::string persistentRoot;
std::string selectedProfile;
std::string targetProfile;
std::string loadError;
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
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (mods.empty() || phase == Phase::Blocked) return;

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
        } else if (phase == Phase::Ready && selectedProfile != key) {
            active.store(false, std::memory_order_release);
            setNativeAchievementWorldActive(false);
            targetProfile = key;
            directory = persistentRoot + "/profiles/" + targetProfile;
            toLoad = mods;
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
