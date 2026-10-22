#include "experiments/csharp/NativeHairSlotProbe.h"

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

namespace bl::csharp::hair {

NativeHairSlotSnapshot ReadNativeHairOccupiedSlots() {
    NativeHairSlotSnapshot snapshot;
    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.object_get_class ||
        !api.class_get_element_class || !api.class_is_valuetype) {
        snapshot.error = "HairSlotProbeIl2CppApiUnavailable";
        return snapshot;
    }

    Il2CppClass* cls = il2cpp::findClass(
        {"Terraria.GameContent", "TextureAssets", {}});
    FieldInfo* mainField = cls ? il2cpp::findField(cls, "PlayerHair") : nullptr;
    FieldInfo* altField = cls ? il2cpp::findField(cls, "PlayerHairAlt") : nullptr;
    if (!mainField || !altField || mainField == altField) {
        snapshot.error = "HairSlotProbeTextureFieldsUnavailable";
        return snapshot;
    }

    Il2CppArray* arrays[2] = {};
    api.field_static_get_value(mainField, &arrays[0]);
    api.field_static_get_value(altField, &arrays[1]);
    for (Il2CppArray* array : arrays) {
        if (!array || array->length < Registry::kVanillaCount ||
            array->length > 16384) {
            snapshot.error = "HairSlotProbeTablesNotReady";
            return snapshot;
        }
        auto* arrayClass = api.object_get_class(
            reinterpret_cast<Il2CppObject*>(array));
        auto* elementClass = arrayClass
            ? api.class_get_element_class(arrayClass) : nullptr;
        if (!elementClass || api.class_is_valuetype(elementClass)) {
            snapshot.error = "HairSlotProbeInvalidElementType";
            return snapshot;
        }
    }

    for (const Il2CppArray* array : arrays) {
        auto* slots = static_cast<Il2CppObject* const*>(
            il2cpp::arrayData(const_cast<Il2CppArray*>(array)));
        for (int id = Registry::kVanillaCount; id <= Registry::kLastType; ++id) {
            if (static_cast<uintptr_t>(id) < array->length && slots[id])
                snapshot.occupied.set(static_cast<size_t>(id));
        }
    }
    snapshot.valid = true;
    return snapshot;
}

} // namespace bl::csharp::hair
