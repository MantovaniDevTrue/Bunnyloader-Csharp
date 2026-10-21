#include "experiments/csharp/NativeTouchIcons.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "content/common/ContentAssets.h"
#include "mods/ModLoader.h"
#include "il2cpp/Api.h"
#include <array>
#include <cstdint>
#include <string>
#include <sys/stat.h>

namespace bl::csharp::touch {
namespace {
struct Icon {
    uint32_t root = 0;
    std::string owner;
    std::string path;
    int width = 0;
    int height = 0;
    uint64_t retryAfter = 0;
};
std::array<Icon, kMaxButtons> icons;
uint64_t polls = 0;

void discard(Icon& icon) {
    auto& api = il2cpp::api();
    if (icon.root) {
        Il2CppObject* asset = api.gchandle_get_target(icon.root);
        if (asset) runtime::content::destroyTextureAsset(asset);
        api.gchandle_free(icon.root);
    }
    icon = {};
}

bool validFile(const std::string& name) {
    struct stat info{};
    return lstat(name.c_str(), &info) == 0 &&
           S_ISREG(info.st_mode) &&
           info.st_size >= 8 && info.st_size <= 1024 * 1024;
}
bool load(Icon& icon, const std::string& owner, const std::string& name) {
    auto& api = il2cpp::api();
    if (polls < icon.retryAfter) return false;
    icon.retryAfter = polls + 120;
    std::string root = mods::rootOf(owner);
    if (root.empty()) {
        // C# também pode usar o id textual do manifesto em vez do UID.
        // Fazemos o lookup apenas no carregamento do PNG, nunca em Draw.
        for (size_t i = 0; i < mods::loadedCount() && i <= UINT16_MAX; ++i) {
            const mods::LoadedMod* mod = mods::get(static_cast<uint16_t>(i));
            if (!mod || mod->internalName != owner) continue;
            if (!root.empty()) return false; // identidade ambígua
            root = mod->dir;
        }
    }
    if (root.empty()) return false;
    std::string image = root + "/content/" + name;
    if (!validFile(image)) {
        image = root + "/icon.png";
        if (!validFile(image)) return false;
    }
    int w = 0, h = 0;
    Il2CppObject* asset = runtime::content::loadTextureAsset(
        image, nullptr, 0, "CSharpTouch:" + owner + "/" + name, &w, &h);
    if (!asset) return false;
    if (w < 1 || h < 1 || w > 256 || h > 256) {
        runtime::content::destroyTextureAsset(asset);
        return false;
    }
    uint32_t handle = api.gchandle_new(asset, false);
    if (!handle) {
        runtime::content::destroyTextureAsset(asset);
        return false;
    }
    icon = {handle, owner, name, w, h, 0};
    return true;
}
}

Il2CppObject* nativeTouchIcon(int id, int* width, int* height) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (id < 0 || id >= kMaxButtons) return nullptr;
    auto& api = il2cpp::api();
    if (!api.gchandle_get_target) return nullptr;
    const Icon& icon = icons[static_cast<size_t>(id)];
    if (!icon.root) return nullptr;
    Il2CppObject* asset = api.gchandle_get_target(icon.root);
    Il2CppObject* texture = asset ? runtime::content::textureAssetValue(asset) : nullptr;
    if (texture) {
        if (width) *width = icon.width;
        if (height) *height = icon.height;
    }
    return texture;
}

void pumpNativeTouchIconsOnGameThread() {
    ++polls;
    if (polls != 1 && polls % 10 != 0) return;
    static bool cachedAny = false;
    if (!cachedAny && registry().count() == 0) return;
    auto& api = il2cpp::api();
    if (!api.gchandle_get_target || !api.gchandle_free ||
        !api.gchandle_new) return;
    static std::array<TouchIconSource, kMaxButtons> sources{};
    const int allocated = registry().iconSources(&sources);
    if (allocated < 0) return;
    int loaded = 0;
    cachedAny = false;
    for (int i = 0; i < allocated; ++i) {
        Icon& icon = icons[static_cast<size_t>(i)];
        const std::string& owner = sources[static_cast<size_t>(i)].owner;
        const std::string& name = sources[static_cast<size_t>(i)].path;
        if (icon.root && (icon.owner != owner || icon.path != name))
            discard(icon);
        if (!owner.empty() && !name.empty() && !icon.root && loaded < 4)
            if (load(icon, owner, name)) ++loaded;
        if (icon.root) cachedAny = true;
    }
}

} // namespace bl::csharp::touch
