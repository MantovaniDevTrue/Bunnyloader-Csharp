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
}

Il2CppObject* nativeTouchIcon(int id, int* width, int* height) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (id < 0 || id >= kMaxButtons) return nullptr;

    auto& api = il2cpp::api();
    if (!api.gchandle_new || !api.gchandle_get_target || !api.gchandle_free)
        return nullptr;
    const std::string owner = registry().ownerId(id);
    const std::string name = registry().iconPath(id);
    Icon& icon = icons[static_cast<size_t>(id)];
    if (owner.empty() || name.empty()) {
        if (icon.root) discard(icon);
        return nullptr;
    }
    if (icon.root && (icon.owner != owner || icon.path != name)) discard(icon);
    if (!icon.root) {
        const std::string root = mods::rootOf(owner);
        if (root.empty()) return nullptr;
        std::string image = root + "/content/" + name;
        if (!validFile(image)) {
            image = root + "/icon.png";
            if (!validFile(image)) return nullptr;
        }
        int w = 0, h = 0;
        Il2CppObject* asset = runtime::content::loadTextureAsset(
            image, nullptr, 0, "CSharpTouch:" + owner + "/" + name, &w, &h);
        if (!asset) return nullptr;
        if (w < 1 || h < 1 || w > 256 || h > 256) {
            runtime::content::destroyTextureAsset(asset);
            return nullptr;
        }
        const uint32_t handle = api.gchandle_new(asset, false);
        if (!handle) {
            runtime::content::destroyTextureAsset(asset);
            return nullptr;
        }
        icon = {handle, owner, name, w, h};
    }
    Il2CppObject* asset = api.gchandle_get_target(icon.root);
    Il2CppObject* texture = asset ? runtime::content::textureAssetValue(asset) : nullptr;
    if (texture) {
        if (width) *width = icon.width;
        if (height) *height = icon.height;
    }
    return texture;
}

void pumpNativeTouchIconsOnGameThread() {
    if (++polls != 1 && polls % 60 != 0) return;
    auto& api = il2cpp::api();
    if (!api.gchandle_get_target || !api.gchandle_free) return;
    for (int i = 0; i < kMaxButtons; ++i) {
        Icon& icon = icons[static_cast<size_t>(i)];
        if (icon.root &&
            (registry().ownerId(i) != icon.owner ||
             registry().iconPath(i) != icon.path))
            discard(icon);
    }
}

} // namespace bl::csharp::touch
