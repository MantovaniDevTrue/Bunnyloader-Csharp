#include "experiments/csharp/NativeHairTextures.h"

#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::hair {
namespace {

struct PendingAsset {
    int type = -1;
    Il2CppObject* main = nullptr;
    Il2CppObject* alt = nullptr;
};

bool validContentRoot(std::string_view root) {
    if (root.empty() || root.size() > 1024 || root.front() != '/') return false;
    if (root.find("..") != std::string_view::npos ||
        root.find('\\') != std::string_view::npos ||
        root.find("//") != std::string_view::npos) return false;
    for (const unsigned char c : root)
        if (c < 32 || c == 127) return false;
    return true;
}

} // namespace

TextureInstallResult InstallNativeHairTextures(
    const Registry& registry,
    const std::function<std::string(std::string_view)>& modContentDirectory) {

    TextureInstallResult result;
    const std::vector<Entry> entries = registry.Snapshot();
    if (entries.empty()) {
        result.installed = true;
        return result;
    }
    if (!modContentDirectory) {
        result.error = "ContentRootResolverMissing";
        return result;
    }

    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.field_static_set_value || !api.gchandle_new ||
        !api.gchandle_free || !api.gchandle_get_target) {
        result.error = "Il2CppArrayOrGcApiUnavailable";
        return result;
    }

    Il2CppClass* cls = il2cpp::findClass({"Terraria.GameContent", "TextureAssets", {}});
    FieldInfo* mainField = cls ? il2cpp::findField(cls, "PlayerHair") : nullptr;
    FieldInfo* altField = cls ? il2cpp::findField(cls, "PlayerHairAlt") : nullptr;
    if (!mainField || !altField || mainField == altField) {
        result.error = "PlayerHairAssetTablesUnavailable";
        return result;
    }

    Il2CppArray* originals[2]{};
    api.field_static_get_value(mainField, &originals[0]);
    api.field_static_get_value(altField, &originals[1]);
    if (!originals[0] || !originals[1] ||
        originals[0]->length < static_cast<uintptr_t>(Registry::kVanillaCount) ||
        originals[1]->length < static_cast<uintptr_t>(Registry::kVanillaCount) ||
        originals[0]->length > 16384 || originals[1]->length > 16384) {
        result.error = "PlayerHairAssetTablesNotReady";
        return result;
    }

    const auto last = static_cast<uintptr_t>(entries.back().type + 1);
    const uintptr_t total = std::max({originals[0]->length, originals[1]->length, last});
    Il2CppArray* grownMain = bl::runtime::TypeTables::resizedCopy(originals[0], total);
    Il2CppArray* grownAlt = bl::runtime::TypeTables::resizedCopy(originals[1], total);
    if (!grownMain || !grownAlt) {
        result.error = "PlayerHairAssetArrayAllocationFailed";
        return result;
    }

    // As referências locais não substituem raízes do GC quando carregar um PNG
    // gera outras alocações IL2CPP. Fixar os arrays até a publicação definitiva.
    const uint32_t mainRoot = api.gchandle_new(reinterpret_cast<Il2CppObject*>(grownMain), false);
    const uint32_t altRoot = api.gchandle_new(reinterpret_cast<Il2CppObject*>(grownAlt), false);
    if (!mainRoot || !altRoot) {
        if (mainRoot) api.gchandle_free(mainRoot);
        if (altRoot) api.gchandle_free(altRoot);
        result.error = "PlayerHairArrayGcRootFailed";
        return result;
    }

    std::vector<PendingAsset> pending;
    pending.reserve(entries.size());
    std::vector<uint32_t> assetRoots;
    std::set<Il2CppObject*> uniqueAssets;
    auto cleanup = [&] {
        // Destruir enquanto os gchandles ainda seguram os assets vivos.
        for (Il2CppObject* asset : uniqueAssets)
            bl::runtime::content::destroyTextureAsset(asset);
        for (const auto root : assetRoots) api.gchandle_free(root);
        api.gchandle_free(altRoot);
        api.gchandle_free(mainRoot);
    };
    auto pinAsset = [&](Il2CppObject* asset) {
        if (!asset || !uniqueAssets.insert(asset).second) return asset != nullptr;
        const uint32_t root = api.gchandle_new(asset, false);
        if (!root) return false;
        assetRoots.push_back(root);
        return true;
    };

    for (const Entry& entry : entries) {
        std::string_view modUid, className;
        if (!splitName(entry.fullName, &modUid, &className) ||
            entry.type < Registry::kVanillaCount || entry.type > Registry::kLastType) {
            result.error = "InvalidNativeHairEntry";
            cleanup();
            return result;
        }
        std::string dir = modContentDirectory(modUid);
        if (!validContentRoot(dir)) {
            result.error = "UnsafeModContentDirectory:" + entry.fullName;
            cleanup();
            return result;
        }
        if (dir.back() != '/') dir.push_back('/');
        const std::string mainPath = dir + entry.texture;
        const std::string altPath = dir + entry.altTexture;
        Il2CppObject* mainAsset = bl::runtime::content::loadTextureAsset(
            mainPath, nullptr, 0, entry.fullName, nullptr, nullptr);
        if (!mainAsset || !pinAsset(mainAsset)) {
            result.error = "MainHairTextureLoadFailed:" + entry.fullName;
            cleanup();
            return result;
        }
        Il2CppObject* altAsset = mainAsset;
        if (entry.altTexture != entry.texture) {
            altAsset = bl::runtime::content::loadTextureAsset(
                altPath, nullptr, 0, entry.fullName + "_Alt", nullptr, nullptr);
            if (!altAsset || !pinAsset(altAsset)) {
                result.error = "AltHairTextureLoadFailed:" + entry.fullName;
                cleanup();
                return result;
            }
        }
        pending.push_back({entry.type, mainAsset, altAsset});
    }

    // As tabelas originais ficam protegidas também: um rollback posterior
    // deve restaurar objetos vivos mesmo se SetValue provocar uma coleta.
    const uint32_t oldMainRoot = api.gchandle_new(
        reinterpret_cast<Il2CppObject*>(originals[0]), false);
    const uint32_t oldAltRoot = api.gchandle_new(
        reinterpret_cast<Il2CppObject*>(originals[1]), false);
    if (!oldMainRoot || !oldAltRoot) {
        if (oldMainRoot) api.gchandle_free(oldMainRoot);
        if (oldAltRoot) api.gchandle_free(oldAltRoot);
        result.error = "VanillaHairArrayGcRootFailed";
        cleanup();
        return result;
    }

    // Não tocar as tabelas originais. Publicar cópias e gravar os elementos
    // via Array.SetValue, que aplica a barreira de escrita do GC.
    api.field_static_set_value(mainField, grownMain);
    api.field_static_set_value(altField, grownAlt);
    bool applied = true;
    for (const PendingAsset& asset : pending) {
        if (!bl::runtime::content::setTableElement(mainField, asset.type, asset.main) ||
            !bl::runtime::content::setTableElement(altField, asset.type, asset.alt)) {
            applied = false;
            break;
        }
    }
    if (!applied) {
        api.field_static_set_value(mainField, originals[0]);
        api.field_static_set_value(altField, originals[1]);
        result.error = "PlayerHairAssetWriteFailedRolledBack";
        cleanup();
        api.gchandle_free(oldAltRoot);
        api.gchandle_free(oldMainRoot);
        return result;
    }

    api.gchandle_free(oldAltRoot);
    api.gchandle_free(oldMainRoot);
    for (const auto root : assetRoots) api.gchandle_free(root);
    api.gchandle_free(altRoot);
    api.gchandle_free(mainRoot);
    result.installed = true;
    result.installedStyles = static_cast<int>(entries.size());
    return result;
}

}
