#include "experiments/csharp/NativeCloudIdProbe.h"

#include "content/common/ContentAssets.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <cstdint>
#include <string>

namespace bl::csharp {
namespace {

NativeCloudIdPlan denied(const char* reason) {
    return {-1, -1, reason};
}

// Static typed Array<T> field, without invoking managed game methods.
// Checking the element type rejects accidental offsets from wrong versions.
Il2CppArray* staticCloudArray(Il2CppClass* owner, const char* name,
                              Il2CppClass* expectedElement) {
    auto& a = il2cpp::api();
    if (!owner || !expectedElement || !a.field_get_type ||
        !a.field_get_flags || !a.class_from_il2cpp_type ||
        !a.class_get_rank || !a.class_get_element_class ||
        !a.field_static_get_value)
        return nullptr;
    FieldInfo* field = il2cpp::findField(owner, name);
    if (!field || !(a.field_get_flags(field) & 0x10u))
        return nullptr;
    Il2CppClass* arrayClass = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!arrayClass || a.class_get_rank(arrayClass) != 1 ||
        a.class_get_element_class(arrayClass) != expectedElement)
        return nullptr;
    Il2CppArray* array = nullptr;
    a.field_static_get_value(field, &array);
    return array && array->obj.klass == arrayClass ? array : nullptr;
}

} // namespace

NativeCloudIdPlan probeNativeCloudIdPlan(int cloudCount,
                                        bool jsReadySetupFinished,
                                        bool mobileLoadMasksFinished) {
    if (!jsReadySetupFinished || !mobileLoadMasksFinished)
        return denied("CloudID: JS setup and LoadMasks must be verified");
    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.class_get_element_class ||
        !api.field_get_type || !api.field_get_flags)
        return denied("CloudID: IL2CPP symbols unavailable");
    auto* cloudId = il2cpp::findClass({"Terraria.ID", "CloudID", {}});
    auto* assets = il2cpp::findClass({"Terraria.GameContent", "TextureAssets", {}});
    auto* masks = il2cpp::findClass({"", "TextureMaskManager", {}});
    auto* maskType = il2cpp::findClass({"", "TextureMask", {}});
    if (!cloudId || !assets || !masks || !maskType)
        return denied("CloudID: native class layout unavailable");

    int vanillaCount = 0;
    std::string readError;
    if (!il2cpp::readNativeId(cloudId, "Count", &vanillaCount, &readError))
        return denied("CloudID: Count field contract mismatch");

    Il2CppArray* textures = staticCloudArray(
        assets, "Cloud", bl::runtime::content::textureAssetClass());
    Il2CppArray* cloudMasks = staticCloudArray(
        masks, "CloudMasks", maskType);
    if (!textures || !cloudMasks || textures->length > 10000 ||
        cloudMasks->length > 10000)
        return denied("CloudID: TextureAssets.Cloud or CloudMasks incomplete");

    return planNativeCloudIds(
        vanillaCount, static_cast<int>(textures->length),
        static_cast<int>(cloudMasks->length), cloudCount,
        jsReadySetupFinished, mobileLoadMasksFinished);
}

} // namespace bl::csharp
