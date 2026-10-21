#include "experiments/csharp/NativeTouchProfiles.h"
#include "experiments/csharp/NativeTouchController.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace bl::csharp::touch {
namespace {

template<typename T> T read(const void* base, size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const char*>(base) + offset, sizeof(value));
    return value;
}

FieldInfo* member(Il2CppClass* cls, const char* name) {
    if (!cls) return nullptr;
    FieldInfo* field = il2cpp::findField(cls, name);
    if (field) return field;
    const std::string backing = std::string("<") + name + ">k__BackingField";
    return il2cpp::findField(cls, backing.c_str());
}
bool offset(Il2CppClass* cls, const char* name, size_t size, bool isRef,
            size_t* out) {
    auto& a = il2cpp::api();
    if (!cls || !a.field_get_flags || !a.field_get_type ||
        !a.class_from_il2cpp_type || !a.class_is_valuetype ||
        !a.field_get_offset || !a.class_instance_size) return false;
    FieldInfo* field = member(cls, name);
    if (!field || (a.field_get_flags(field) & 0x10u)) return false;
    Il2CppClass* type = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!type || a.class_is_valuetype(type) == isRef) return false;
    const size_t position = a.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + size > a.class_instance_size(cls)) return false;
    *out = position;
    return true;
}
bool numeric(Il2CppClass* cls, const char* field, size_t* out) {
    auto& a = il2cpp::api();
    if (!offset(cls, field, 4, false, out) ||
        !a.type_get_name || !a.il2cpp_free) return false;
    FieldInfo* memberField = member(cls, field);
    Il2CppClass* type = a.class_from_il2cpp_type(a.field_get_type(memberField));
    if (a.class_is_enum && a.class_is_enum(type)) {
        if (!a.class_enum_basetype) return false;
        char* underlying = a.type_get_name(a.class_enum_basetype(type));
        const bool ok = underlying && std::strcmp(underlying, "System.Int32") == 0;
        if (underlying) a.il2cpp_free(underlying);
        return ok;
    }
    char* name = a.type_get_name(a.field_get_type(memberField));
    const bool ok = name && std::strcmp(name, "System.Int32") == 0;
    if (name) a.il2cpp_free(name);
    return ok;
}
Il2CppObject* staticSingleton(Il2CppClass* cls) {
    auto& a = il2cpp::api();
    if (!cls || !a.field_static_get_value || !a.field_get_flags) return nullptr;
    FieldInfo* field = member(cls, "Instance");
    if (field && (a.field_get_flags(field) & 0x10u)) {
        Il2CppObject* result = nullptr;
        a.field_static_get_value(field, &result);
        return result;
    }
    if (!a.class_get_method_from_name || !a.method_is_instance ||
        !a.runtime_invoke) return nullptr;
    const MethodInfo* getter = a.class_get_method_from_name(cls, "get_Instance", 0);
    if (!getter || a.method_is_instance(getter)) return nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* result = a.runtime_invoke(getter, nullptr, nullptr, &error);
    return error ? nullptr : result;
}
Il2CppObject* activeProfile() {
    auto& a = il2cpp::api();
    Il2CppClass* type = il2cpp::findClassQuiet("", "VirtualControlsStyles_Layout");
    Il2CppObject* styles = staticSingleton(type);
    if (!styles || !a.object_get_class) return nullptr;
    Il2CppClass* actual = a.object_get_class(styles);
    size_t position = 0;
    if (offset(actual, "_activeProfile", sizeof(void*), true, &position) ||
        offset(actual, "ActiveProfile", sizeof(void*), true, &position))
        return read<Il2CppObject*>(styles, position);
    if (!a.class_get_method_from_name || !a.method_is_instance ||
        !a.runtime_invoke) return nullptr;
    const MethodInfo* getter = a.class_get_method_from_name(actual, "get_ActiveProfile", 0);
    if (!getter || !a.method_is_instance(getter)) return nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* value = a.runtime_invoke(getter, styles, nullptr, &error);
    return error ? nullptr : value;
}
bool objectArray(Il2CppObject* owner, const char* field, Il2CppArray** out) {
    auto& a = il2cpp::api();
    if (!owner || !a.object_get_class || !a.class_get_rank ||
        !a.class_get_element_class) return false;
    size_t position = 0;
    if (!offset(a.object_get_class(owner), field, sizeof(void*), true, &position))
        return false;
    Il2CppArray* value = read<Il2CppArray*>(owner, position);
    if (!value || value->length > 256) return false;
    Il2CppClass* array = a.object_get_class(reinterpret_cast<Il2CppObject*>(value));
    if (!array || a.class_get_rank(array) != 1 ||
        !a.class_get_element_class(array)) return false;
    *out = value;
    return true;
}
Il2CppObject* arrayAt(Il2CppArray* array, int index) {
    auto& a = il2cpp::api();
    if (!array || index < 0 || static_cast<uintptr_t>(index) >= array->length ||
        !a.object_get_class || !a.class_get_element_class || !a.class_is_valuetype)
        return nullptr;
    Il2CppClass* element = a.class_get_element_class(
        a.object_get_class(reinterpret_cast<Il2CppObject*>(array)));
    if (!element || a.class_is_valuetype(element)) return nullptr;
    auto* values = static_cast<Il2CppObject**>(arrayData(array));
    return values[index];
}
bool numericGroup(Il2CppObject* binding, SlotAction* result) {
    Il2CppArray* array = nullptr;
    if (!objectArray(binding, "GroupActions", &array)) return false;
    if (array->length > result->group.size()) return false;
    auto& a = il2cpp::api();
    Il2CppClass* element = a.class_get_element_class(a.object_get_class(
        reinterpret_cast<Il2CppObject*>(array)));
    if (!element || !a.class_value_size || !a.class_is_valuetype(element))
        return false;
    uint32_t align = 0;
    if (a.class_value_size(element, &align) != 4) return false;
    result->groupCount = static_cast<int>(array->length);
    std::memcpy(result->group.data(), arrayData(array),
                array->length * sizeof(int32_t));
    return true;
}
bool styleOf(Il2CppObject* profile, int slot, int* style) {
    auto& a = il2cpp::api();
    if (!a.object_get_class) return false;
    size_t hwOff = 0;
    if (!offset(a.object_get_class(profile), "HardwareConfig",
                sizeof(void*), true, &hwOff)) return false;
    Il2CppObject* hardware = read<Il2CppObject*>(profile, hwOff);
    Il2CppArray* slots = nullptr;
    if (!hardware || !objectArray(hardware, "Slots", &slots)) return false;
    Il2CppObject* config = arrayAt(slots, slot);
    if (!config) return false;
    size_t stOff = 0;
    if (!offset(a.object_get_class(config), "Style",
                sizeof(void*), true, &stOff)) return false;
    Il2CppObject* styleObject = read<Il2CppObject*>(config, stOff);
    size_t typeOff = 0;
    if (!styleObject || !numeric(a.object_get_class(styleObject),
                                 "ControlType", &typeOff)) return false;
    *style = read<int32_t>(styleObject, typeOff);
    return true;
}
}

namespace {
bool slotFromProfile(Il2CppObject* profile, Il2CppArray* controls,
                     int slot, SlotAction* result) {
    if (!profile || !controls || !result || slot < 0 ||
        static_cast<uintptr_t>(slot) >= controls->length) return false;
    auto& a = il2cpp::api();
    if (!a.object_get_class || !a.class_from_il2cpp_type) return false;
    Il2CppObject* binding = arrayAt(controls, slot);
    if (!binding) return false;
    size_t actionOffset = 0;
    if (!numeric(a.object_get_class(binding), "Action", &actionOffset))
        return false;
    SlotAction temp;
    temp.action = read<int32_t>(binding, actionOffset);
    if (!styleOf(profile, slot, &temp.style)) return false;
    if (temp.style == 2 && !numericGroup(binding, &temp)) return false;
    *result = temp;
    return true;
}
}

const void* nativeTouchActiveProfileIdentity() {
    return activeProfile();
}

bool nativeTouchSlot(int slot, SlotAction* result) {
    if (!result || slot < 0 || slot >= 256) return false;
    Il2CppObject* profile = activeProfile();
    Il2CppArray* controls = nullptr;
    if (!profile || !objectArray(profile, "Controls", &controls)) return false;
    return slotFromProfile(profile, controls, slot, result);
}
bool nativeTouchSlotId(int slot, std::string* result) {
    if (!result || slot < 0 || slot >= 256) return false;
    auto& api = il2cpp::api();
    if (!api.object_get_class) return false;
    Il2CppObject* profile = activeProfile();
    if (!profile) return false;
    size_t offsetField = 0;
    if (!offset(api.object_get_class(profile), "HardwareConfig",
                sizeof(void*), true, &offsetField)) return false;
    Il2CppObject* hardware = read<Il2CppObject*>(profile, offsetField);
    Il2CppArray* slots = nullptr;
    if (!hardware || !objectArray(hardware, "Slots", &slots)) return false;
    Il2CppObject* entry = arrayAt(slots, slot);
    if (!entry || !offset(api.object_get_class(entry), "SlotId",
                          sizeof(void*), true, &offsetField)) return false;
    Il2CppString* id = read<Il2CppString*>(entry, offsetField);
    if (!id || id->length < 1 || id->length > 100) return false;
    std::string value;
    value.reserve(id->length);
    for (int i = 0; i < id->length; ++i) {
        char16_t character = id->chars[i];
        if (character < 32 || character >= 127) return false;
        value.push_back(static_cast<char>(character));
    }
    *result = std::move(value);
    return true;
}

bool nativeTouchActiveSnapshot(ActiveTouchSnapshot* result) {
    if (!result) return false;
    ActiveTouchSnapshot snapshot;
    snapshot.actionSlots.fill(-1);
    Il2CppObject* profile = activeProfile();
    Il2CppArray* controls = nullptr;
    if (!profile || !objectArray(profile, "Controls", &controls)) return false;
    const int count = static_cast<int>(controls->length);
    for (int slot = 0; slot < count; ++slot) {
        SlotAction action;
        if (!slotFromProfile(profile, controls, slot, &action)) continue;
        int id = nativeTouchIndexByAction(action.action);
        if (id >= 0 && id < kMaxButtons) {
            snapshot.bound[static_cast<size_t>(id)] = true;
            snapshot.actionSlots[static_cast<size_t>(id)] = slot;
        }
        for (int j = 0; j < action.groupCount; ++j) {
            id = nativeTouchIndexByAction(action.group[static_cast<size_t>(j)]);
            if (id >= 0 && id < kMaxButtons)
                snapshot.bound[static_cast<size_t>(id)] = true;
        }
    }
    snapshot.ready = true;
    *result = snapshot;
    return true;
}

} // namespace bl::csharp::touch
