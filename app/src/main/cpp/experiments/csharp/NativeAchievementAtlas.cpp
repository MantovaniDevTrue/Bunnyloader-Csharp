#include "experiments/csharp/NativeAchievementAtlas.h"
#include "experiments/csharp/NativeAchievementAtlasPlan.h"
#include "content/common/ContentAssets.h"
#include "mods/ModLoader.h"
#include "il2cpp/Api.h"

#include <sys/stat.h>

namespace bl::csharp::achievement {
namespace {

void dispose(std::map<std::string, uint32_t>& loaded) {
    auto& a = il2cpp::api();
    for (const auto& [name, handle] : loaded) {
        if (!handle) continue;
        Il2CppObject* asset = a.gchandle_get_target
            ? a.gchandle_get_target(handle) : nullptr;
        if (asset) runtime::content::destroyTextureAsset(asset);
        if (a.gchandle_free) a.gchandle_free(handle);
    }
    loaded.clear();
}

}

bool NativeAchievementAtlases::prepare(
    const std::string& mod, const std::string& revision,
    const std::vector<Descriptor>& achievements, std::string* error) {
    if (!error) return false;
    error->clear();
    if (revision.empty() || revision.size() > 4096) {
        *error = "InvalidRevision";
        return false;
    }
    const auto existing = owners_.find(mod);
    if (existing != owners_.end() && existing->second.revision == revision)
        return true;
    auto& api = il2cpp::api();
    if (!api.gchandle_new || !api.gchandle_free || !api.gchandle_get_target) {
        *error = "NativeGCHandleUnavailable";
        return false;
    }
    std::vector<AtlasEntry> plan;
    if (!planAchievementAtlases(mod, achievements, &plan)) {
        *error = "InvalidAtlasMetadata";
        return false;
    }
    const std::string root = mods::rootOf(mod);
    if (root.empty()) {
        *error = "ModRootUnavailable";
        return false;
    }
    std::map<std::string, uint32_t> staged;
    for (const AtlasEntry& entry : plan) {
        const std::string path = root + "/" + entry.filename;
        struct stat file{};
        if (stat(path.c_str(), &file) != 0 || !S_ISREG(file.st_mode) ||
            file.st_size < 1 || file.st_size > 1024 * 1024) {
            *error = "AchievementAtlasMissing";
            dispose(staged);
            return false;
        }
        int width = 0, height = 0;
        Il2CppObject* asset = runtime::content::loadTextureAsset(
            path, nullptr, 0, "CSharpAchievement:" + mod + ":" + entry.texture,
            &width, &height);
        if (!asset) {
            *error = "AchievementAtlasDecodeFailed";
            dispose(staged);
            return false;
        }
        if (width != 592 || height != 64) {
            runtime::content::destroyTextureAsset(asset);
            *error = "AchievementAtlasMustBe592x64";
            dispose(staged);
            return false;
        }
        uint32_t handle = api.gchandle_new(asset, false);
        if (!handle) {
            runtime::content::destroyTextureAsset(asset);
            *error = "AchievementAtlasRootFailed";
            dispose(staged);
            return false;
        }
        staged.emplace(entry.texture, handle);
    }
    Owner replacement;
    replacement.revision = revision;
    for (const auto& [name, handle] : staged)
        replacement.textures.emplace(name, Atlas{handle});
    staged.clear();

    retire(mod);
    owners_.emplace(mod, std::move(replacement));
    return true;
}

Il2CppObject* NativeAchievementAtlases::asset(
    const std::string& mod, const std::string& texture) const {
    auto modIt = owners_.find(mod);
    if (modIt == owners_.end()) return nullptr;
    auto assetIt = modIt->second.textures.find(texture);
    if (assetIt == modIt->second.textures.end() ||
        !il2cpp::api().gchandle_get_target) return nullptr;
    return il2cpp::api().gchandle_get_target(assetIt->second.root);
}

void NativeAchievementAtlases::retire(const std::string& mod) {
    auto it = owners_.find(mod);
    if (it == owners_.end()) return;
    std::map<std::string, uint32_t> handles;
    for (auto& [name, asset] : it->second.textures)
        handles.emplace(name, asset.root);
    dispose(handles);
    owners_.erase(it);
}

void NativeAchievementAtlases::reset() {
    while (!owners_.empty()) retire(owners_.begin()->first);
}

NativeAchievementAtlases& nativeAchievementAtlases() {
    static NativeAchievementAtlases instance;
    return instance;
}

}
