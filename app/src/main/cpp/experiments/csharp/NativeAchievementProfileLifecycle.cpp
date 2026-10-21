#include "experiments/csharp/NativeAchievementProfileLifecycle.h"

#include "experiments/csharp/NativeAchievementProfile.h"
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
enum class Phase { Pending, Loading, Completed, Ready, Blocked };
std::mutex mutex;
std::vector<ModInfo> mods;
std::vector<ManifestInfo> loaded;
std::string persistentRoot;
std::string selectedProfile;
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

        if (!selectedProfile.empty() && selectedProfile != key) {
            phase = Phase::Blocked;
            active.store(false, std::memory_order_release);
            setNativeAchievementWorldActive(false);
            if (!reportedMismatch) {
                BL_WARN("[CSharp][AchievementLoader] personagem mudou; conteudo de conquistas pausado ate reiniciar");
                reportedMismatch = true;
            }
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
        } else if (phase == Phase::Ready) {
            // A flag pode ter sido suspensa se o getter ficou indisponível.
            active.store(true, std::memory_order_release);
            reportedMismatch = false;
            return;
        }
    }
    if (!directory.empty()) {
        std::thread([directory = std::move(directory),
                     mods = std::move(toLoad)]() mutable {
            loadProfile(std::move(directory), std::move(mods));
        }).detach();
    }
}

}
