#include "experiments/csharp/NativeCloudAssets.h"
#include "experiments/csharp/NativeCloudHooks.h"

#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

using bl::runtime::content::loadTextureAsset;
using bl::runtime::content::destroyTextureAsset;

struct Session {
    FieldInfo* textureTable = nullptr;
    FieldInfo* maskTable = nullptr;
    int firstType = 0;
    int lastExclusive = 0;
    uint32_t oldTextures = 0, oldMasks = 0;
    uint32_t newTextures = 0, newMasks = 0;
    std::vector<uint32_t> loadedAssets;
    std::vector<uint32_t> loadedMasks;

    uint32_t pin(Il2CppObject* value) {
        auto& a = il2cpp::api();
        return value && a.gchandle_new ? a.gchandle_new(value, false) : 0;
    }

    Il2CppArray* array(uint32_t handle) const {
        return reinterpret_cast<Il2CppArray*>(il2cpp::api().gchandle_get_target(handle));
    }

    // Run only after both published arrays have been removed from the game.
    // A Unity texture must not be destroyed while a cloud is still drawing it.
    void cleanup() {
        auto& a = il2cpp::api();
        for (auto handle : loadedAssets) {
            if (auto* asset = a.gchandle_get_target(handle))
                destroyTextureAsset(asset);
            a.gchandle_free(handle);
        }
        for (auto handle : loadedMasks) a.gchandle_free(handle);
        for (auto handle : {oldTextures, oldMasks, newTextures, newMasks})
            if (handle) a.gchandle_free(handle);
        loadedAssets.clear();
        loadedMasks.clear();
        oldTextures = oldMasks = newTextures = newMasks = 0;
    }
};

Session* g_installed = nullptr;
std::thread::id g_gameThread;

bool fail(std::string* error, const char* reason) {
    if (error) *error = reason;
    return false;
}

Il2CppArray* table(FieldInfo* field) {
    if (!field || !il2cpp::api().field_static_get_value) return nullptr;
    Il2CppArray* result = nullptr;
    il2cpp::api().field_static_get_value(field, &result);
    return result;
}

bool invoke(const MethodInfo* method, void* self, void** args) {
    if (!method || !il2cpp::api().runtime_invoke) return false;
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(method, self, args, &exception);
    return exception == nullptr;
}

const MethodInfo* setValueMethod() {
    auto* arrayClass = il2cpp::findClass({"System", "Array", {}});
    return arrayClass ? il2cpp::findMethodBySignature(
        arrayClass, il2cpp::parseSignature(
            "void SetValue(object value, int index)")) : nullptr;
}

bool setValue(const MethodInfo* method, Il2CppArray* array, int index,
              Il2CppObject* value) {
    if (!method || !array || index < 0 ||
        static_cast<uintptr_t>(index) >= array->length)
        return false;
    // Array.SetValue performs the write barrier. Writing a pointer directly
    // into arrayData() is unsafe under the incremental IL2CPP collector.
    void* args[] = {value, &index};
    return invoke(method, array, args);
}

bool validStaticArrayField(FieldInfo* field, Il2CppClass* elementType) {
    auto& a = il2cpp::api();
    if (!field || !elementType || !a.field_get_flags ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_rank || !a.class_get_element_class)
        return false;
    if (!(a.field_get_flags(field) & 0x10u)) return false;
    Il2CppClass* fieldType = a.class_from_il2cpp_type(a.field_get_type(field));
    return fieldType && a.class_get_rank(fieldType) == 1 &&
        a.class_get_element_class(fieldType) == elementType;
}

// The original JS CloudLoader accesses the two arrays by their exact typed
// static fields. Reject renamed or mismatched fields before staging handles.
bool preflight(FieldInfo** textureTable, FieldInfo** maskTable,
               Il2CppClass** maskClass, const MethodInfo** maskCtor,
               int* srcWidth, int* srcHeight, int* outputWidth,
               std::string* error) {
    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.field_static_set_value ||
        !a.object_new || !a.runtime_invoke || !a.gchandle_new ||
        !a.gchandle_free || !a.gchandle_get_target ||
        !a.object_get_class || !a.class_get_element_class ||
        !a.class_get_rank || !a.array_new)
        return fail(error, "Stage=Resolve Reason=MissingIL2CPPExport");

    auto* textureAssets = il2cpp::findClass(
        {"Terraria.GameContent", "TextureAssets", {}});
    auto* masks = il2cpp::findClass({"", "TextureMaskManager", {}});
    *maskClass = il2cpp::findClass({"", "TextureMask", {}});
    *textureTable = textureAssets ? il2cpp::findField(textureAssets, "Cloud") : nullptr;
    *maskTable = masks ? il2cpp::findField(masks, "CloudMasks") : nullptr;
    *maskCtor = *maskClass
        ? a.class_get_method_from_name(*maskClass, ".ctor", 0) : nullptr;
    if (!*textureTable || !*maskTable || !*maskClass || !*maskCtor ||
        !setValueMethod() ||
        !validStaticArrayField(*textureTable,
            bl::runtime::content::textureAssetClass()) ||
        !validStaticArrayField(*maskTable, *maskClass))
        return fail(error, "Stage=Resolve Reason=CloudTextureOrMaskFieldContractInvalid");

    if (!il2cpp::nativeFieldContract(*maskClass, "srcWidth", "System.Int32",
            false, 4, nullptr, srcWidth, error) ||
        !il2cpp::nativeFieldContract(*maskClass, "srcHeight", "System.Int32",
            false, 4, nullptr, srcHeight, error))
        return false;

    // On Android, the mask generated with outputWidth==0 is intentionally
    // empty. The new cloud still renders; it simply does not occlude the sun.
    // Must be able to prove that this mask cannot occlude the sun.
    // An absent outputWidth field is not an invitation to assume zero.
    if (!il2cpp::nativeFieldContract(*maskClass, "outputWidth",
            "System.Int32", false, 4, nullptr, outputWidth, error))
        return false;
    return true;
}

bool sameArray(FieldInfo* field, Il2CppArray* expected) {
    return table(field) == expected;
}

} // namespace

bool nativeCloudAssetsInstalled() {
    return g_installed != nullptr;
}

bool installNativeCloudAssets(NativeCloudRegistry* registry,
                              const std::map<std::string, std::string>& modDirectories,
                              std::string* error) {
    if (error) error->clear();
    if (g_installed || !registry || registry->count() == 0 ||
        registry->count() > NativeCloudWeightedPool::kMaxModClouds ||
        registry->firstType() <= 0)
        return fail(error, "Stage=Begin Reason=InvalidRegistryOrAlreadyInstalled");

    FieldInfo *textureTable = nullptr, *maskTable = nullptr;
    Il2CppClass* maskClass = nullptr;
    const MethodInfo* maskCtor = nullptr;
    int srcWidth = -1, srcHeight = -1, outputWidth = -1;
    if (!preflight(&textureTable, &maskTable, &maskClass, &maskCtor,
                   &srcWidth, &srcHeight, &outputWidth, error))
        return false;

    auto& a = il2cpp::api();
    Il2CppArray* existingTextures = table(textureTable);
    Il2CppArray* existingMasks = table(maskTable);
    const int firstType = registry->firstType();
    const int newLength = firstType + static_cast<int>(registry->count());
    if (!existingTextures || !existingMasks ||
        a.class_get_rank(existingTextures->obj.klass) != 1 ||
        a.class_get_rank(existingMasks->obj.klass) != 1 ||
        existingTextures->length < static_cast<uintptr_t>(firstType) ||
        existingMasks->length < static_cast<uintptr_t>(firstType) ||
        existingTextures->length > 16384 || existingMasks->length > 16384 ||
        newLength > 16384 ||
        a.class_get_element_class(existingMasks->obj.klass) != maskClass ||
        a.class_get_element_class(existingTextures->obj.klass) !=
            bl::runtime::content::textureAssetClass())
        return fail(error, "Stage=Resolve Reason=IncompatibleVanillaCloudArrays");

    // A previous JS cloud mod may own one of these positions already. Do not
    // overwrite it: cross-loader ID coordination is required before enabling.
    for (int type = firstType; type < newLength; ++type) {
        if ((static_cast<uintptr_t>(type) < existingTextures->length &&
             static_cast<Il2CppObject**>(arrayData(existingTextures))[type]) ||
            (static_cast<uintptr_t>(type) < existingMasks->length &&
             static_cast<Il2CppObject**>(arrayData(existingMasks))[type]))
            return fail(error, "Stage=Reserve Reason=CloudTypeAlreadyOwned");
    }

    auto stage = std::make_unique<Session>();
    stage->textureTable = textureTable;
    stage->maskTable = maskTable;
    stage->firstType = firstType;
    stage->lastExclusive = newLength;
    stage->oldTextures = stage->pin(reinterpret_cast<Il2CppObject*>(existingTextures));
    stage->oldMasks = stage->pin(reinterpret_cast<Il2CppObject*>(existingMasks));
    if (!stage->oldTextures || !stage->oldMasks) {
        stage->cleanup();
        return fail(error, "Stage=Reserve Reason=CannotPinVanillaArrays");
    }

    const uintptr_t textureSize =
        std::max(static_cast<uintptr_t>(newLength), existingTextures->length);
    const uintptr_t maskSize =
        std::max(static_cast<uintptr_t>(newLength), existingMasks->length);
    Il2CppArray* enlargedTextures =
        bl::runtime::TypeTables::resizedCopy(existingTextures, textureSize);
    stage->newTextures = stage->pin(reinterpret_cast<Il2CppObject*>(enlargedTextures));
    if (!stage->newTextures) {
        stage->cleanup();
        return fail(error, "Stage=Reserve Reason=CannotPinNewTextureArray");
    }
    // Pin immediately before another managed allocation can trigger a GC.
    Il2CppArray* enlargedMasks =
        bl::runtime::TypeTables::resizedCopy(existingMasks, maskSize);
    stage->newMasks = stage->pin(reinterpret_cast<Il2CppObject*>(enlargedMasks));
    if (!stage->newTextures || !stage->newMasks) {
        stage->cleanup();
        return fail(error, "Stage=Reserve Reason=CannotGrowCloudArrays");
    }

    const MethodInfo* setter = setValueMethod();
    for (int type = firstType; type < newLength; ++type) {
        const NativeCloudRegistry::Entry* entry = registry->find(type);
        if (!entry) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=MissingRegisteredCloud");
        }
        auto owner = modDirectories.find(entry->modId);
        if (owner == modDirectories.end() || owner->second.empty()) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=MissingModDirectory");
        }
        const std::string filename = owner->second + "/content/" +
            entry->texture + ".png";
        int width = 0, height = 0;
        Il2CppObject* texture = loadTextureAsset(filename, nullptr, 0,
            "Images/Cloud_" + std::to_string(type), &width, &height);
        if (!texture || width <= 0 || width > 4096 ||
            height <= 0 || height > 4096) {
            if (texture) destroyTextureAsset(texture);
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=InvalidOrMissingCloudPNG");
        }
        uint32_t texturePin = stage->pin(texture);
        if (!texturePin) {
            destroyTextureAsset(texture);
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=CannotPinTextureAsset");
        }
        stage->loadedAssets.push_back(texturePin);

        Il2CppObject* mask = a.object_new(maskClass);
        if (!mask || !invoke(maskCtor, mask, nullptr)) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=CannotConstructTextureMask");
        }
        uint32_t maskPin = stage->pin(mask);
        if (!maskPin) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=CannotPinCloudMask");
        }
        stage->loadedMasks.push_back(maskPin);
        *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mask) + srcWidth) = width;
        *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mask) + srcHeight) = height;
        if (outputWidth >= 0 &&
            *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mask) + outputWidth) != 0) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=MaskMustNotOccludeSun");
        }

        if (!setValue(setter, stage->array(stage->newMasks), type, mask) ||
            !setValue(setter, stage->array(stage->newTextures), type, texture)) {
            stage->cleanup();
            return fail(error, "Stage=Prepare Reason=ArraySetValueRejected");
        }
    }

    // Fail if another loader changed the arrays during preparation. Nothing
    // reaches the renderer until both fully populated arrays are ready.
    if (!sameArray(textureTable, stage->array(stage->oldTextures)) ||
        !sameArray(maskTable, stage->array(stage->oldMasks))) {
        stage->cleanup();
        return fail(error, "Stage=Publish Reason=CloudArraysChangedDuringPrepare");
    }

    // Publish masks FIRST. No cloud with a new type can be picked yet, and
    // the horizon must never see TextureAssets.Cloud without CloudMasks.
    a.field_static_set_value(maskTable, stage->array(stage->newMasks));
    if (!sameArray(maskTable, stage->array(stage->newMasks))) {
        a.field_static_set_value(maskTable, stage->array(stage->oldMasks));
        stage->cleanup();
        return fail(error, "Stage=Publish Reason=CloudMasksNotPublished");
    }

    a.field_static_set_value(textureTable, stage->array(stage->newTextures));
    if (!sameArray(textureTable, stage->array(stage->newTextures))) {
        a.field_static_set_value(textureTable, stage->array(stage->oldTextures));
        a.field_static_set_value(maskTable, stage->array(stage->oldMasks));
        // If rollback did not succeed, keep assets alive instead of
        // destroying a texture that might still be visible in the sky.
        if (!sameArray(textureTable, stage->array(stage->oldTextures)) ||
            !sameArray(maskTable, stage->array(stage->oldMasks))) {
            g_gameThread = std::this_thread::get_id();
            g_installed = stage.release();
            return fail(error, "Stage=Rollback Reason=ArraysStillPublished");
        }
        stage->cleanup();
        return fail(error, "Stage=Publish Reason=CloudTexturesNotPublished");
    }

    for (int type = firstType; type < newLength; ++type) {
        const auto* entry = registry->find(type);
        auto* mask = static_cast<Il2CppObject**>(arrayData(stage->array(stage->newMasks)))[type];
        auto* texture = static_cast<Il2CppObject**>(arrayData(stage->array(stage->newTextures)))[type];
        int32_t width = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mask) + srcWidth);
        int32_t height = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mask) + srcHeight);
        if (!entry || !registry->setInstalled(type,
                static_cast<int>(stage->array(stage->newTextures)->length),
                static_cast<int>(stage->array(stage->newMasks)->length),
                texture != nullptr, mask != nullptr, width, height, 0)) {
            // Defensive rollback of a partial registry installation.
            for (int t = firstType; t < newLength; ++t)
                registry->setInstalled(t, 0, 0, false, false, 0, 0, -1);
            a.field_static_set_value(textureTable, stage->array(stage->oldTextures));
            a.field_static_set_value(maskTable, stage->array(stage->oldMasks));
            if (!sameArray(textureTable, stage->array(stage->oldTextures)) ||
                !sameArray(maskTable, stage->array(stage->oldMasks))) {
                g_gameThread = std::this_thread::get_id();
                g_installed = stage.release();
                return fail(error, "Stage=Rollback Reason=RegistryValidationAndArraysStillPublished");
            }
            stage->cleanup();
            return fail(error, "Stage=Publish Reason=RenderGateRejectedCloud");
        }
    }

    g_gameThread = std::this_thread::get_id();
    g_installed = stage.release();
    return true;
}

// Read-only check performed on the Terraria game thread immediately
// before the old cloud tables are restored. Even one active mod cloud
// must prevent destroying the Texture2D it is still rendering.
bool hasActiveNativeClouds(const Session& session, bool* trustworthy) {
    if (trustworthy) *trustworthy = false;
    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.class_get_rank ||
        !a.class_get_element_class)
        return true;
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* cloud = il2cpp::findClass({"Terraria", "Cloud", {}});
    if (!main || !cloud) return true;
    FieldInfo* cloudsField = il2cpp::findField(main, "cloud");
    if (!cloudsField) return true;
    int activeOffset = -1, typeOffset = -1;
    std::string unused;
    if (!il2cpp::nativeFieldContract(
            cloud, "active", "System.Boolean", false, 1,
            nullptr, &activeOffset, &unused) ||
        !il2cpp::nativeFieldContract(
            cloud, "type", "System.Int32", false, 4,
            nullptr, &typeOffset, &unused))
        return true;
    Il2CppArray* clouds = nullptr;
    a.field_static_get_value(cloudsField, &clouds);
    if (!clouds || clouds->length < 200 || clouds->length > 2000 ||
        a.class_get_rank(clouds->obj.klass) != 1 ||
        a.class_get_element_class(clouds->obj.klass) != cloud)
        return true;
    Il2CppObject** items = static_cast<Il2CppObject**>(arrayData(clouds));
    for (uintptr_t i = 0; i < clouds->length; ++i) {
        Il2CppObject* item = items[i];
        if (!item) continue;
        bool active = *reinterpret_cast<uint8_t*>(
            reinterpret_cast<char*>(item) + activeOffset) != 0;
        if (!active) continue;
        int type = *reinterpret_cast<int32_t*>(
            reinterpret_cast<char*>(item) + typeOffset);
        if (type >= session.firstType && type < session.lastExclusive)
            return true;
    }
    if (trustworthy) *trustworthy = true;
    return false;
}

bool uninstallNativeCloudAssets(std::string* error) {
    if (error) error->clear();
    if (!g_installed) return true;
    if (std::this_thread::get_id() != g_gameThread)
        return fail(error, "Stage=Uninstall Reason=NotGameThread");

    auto& a = il2cpp::api();
    auto* session = g_installed;
    if (nativeCloudHooksEnabled())
        return fail(error, "Stage=Uninstall Reason=CloudSelectionHooksStillRunning");
    bool cloudStateVerified = false;
    const bool activeModCloud = hasActiveNativeClouds(*session, &cloudStateVerified);
    if (!cloudStateVerified || activeModCloud)
        return fail(error, "Stage=Uninstall Reason=ModCloudActiveOrStateUnverified");
    if (!sameArray(session->textureTable, session->array(session->newTextures)) ||
        !sameArray(session->maskTable, session->array(session->newMasks)))
        return fail(error, "Stage=Uninstall Reason=ArraysChangedByAnotherLoader");

    // Stop selection and clear active clouds BEFORE calling this function.
    a.field_static_set_value(session->textureTable, session->array(session->oldTextures));
    a.field_static_set_value(session->maskTable, session->array(session->oldMasks));
    if (!sameArray(session->textureTable, session->array(session->oldTextures)) ||
        !sameArray(session->maskTable, session->array(session->oldMasks)))
        return fail(error, "Stage=Uninstall Reason=CouldNotRestoreVanillaArrays");

    session->cleanup();
    delete session;
    g_installed = nullptr;
    return true;
}

} // namespace bl::csharp
