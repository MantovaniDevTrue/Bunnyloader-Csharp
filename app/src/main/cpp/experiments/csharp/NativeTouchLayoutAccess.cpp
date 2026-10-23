#include "experiments/csharp/NativeTouchLayoutAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::touch {
namespace {
template<typename T> T read(const void* object, size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const char*>(object) + offset, sizeof(value));
    return value;
}
FieldInfo* member(Il2CppClass* cls, const char* name) {
    FieldInfo* field = cls ? il2cpp::findField(cls, name) : nullptr;
    if (field) return field;
    const std::string backing = std::string("<") + name + ">k__BackingField";
    return cls ? il2cpp::findField(cls, backing.c_str()) : nullptr;
}
bool offset(Il2CppClass* cls, const char* name, size_t width,
            bool reference, size_t* result) {
    auto& a = il2cpp::api();
    if (!cls || !a.field_get_flags || !a.field_get_type ||
        !a.class_from_il2cpp_type || !a.class_is_valuetype ||
        !a.field_get_offset || !a.class_instance_size) return false;
    FieldInfo* field = member(cls, name);
    if (!field || (a.field_get_flags(field) & 0x10u)) return false;
    Il2CppClass* type = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!type || a.class_is_valuetype(type) == reference) return false;
    size_t position = a.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + width > a.class_instance_size(cls)) return false;
    *result = position;
    return true;
}
bool stringValue(Il2CppString* value, std::string* out, size_t limit = 128) {
    if (!value || value->length < 1 || value->length > static_cast<int32_t>(limit))
        return false;
    out->clear();
    const int n = value->length;
    for (int i = 0; i < n; ++i) {
        char16_t c = value->chars[i];
        if (c < 32 || c >= 127) return false;
        out->push_back(static_cast<char>(c));
    }
    return true;
}
bool fieldString(Il2CppObject* object, const char* name, std::string* out) {
    auto& a = il2cpp::api();
    if (!object || !a.object_get_class || !a.class_get_name) return false;
    size_t pos = 0;
    Il2CppClass* cls = a.object_get_class(object);
    if (!offset(cls, name, sizeof(void*), true, &pos)) return false;
    Il2CppString* value = read<Il2CppString*>(object, pos);
    return stringValue(value, out);
}
bool arrayField(Il2CppObject* obj, const char* name,
                Il2CppArray** result, bool references) {
    auto& a = il2cpp::api();
    if (!obj || !a.object_get_class || !a.class_get_rank ||
        !a.class_get_element_class || !a.class_is_valuetype) return false;
    size_t pos = 0;
    if (!offset(a.object_get_class(obj), name, sizeof(void*), true, &pos))
        return false;
    auto* array = read<Il2CppArray*>(obj, pos);
    if (!array || array->length > 256) return false;
    Il2CppClass* cls = a.object_get_class(reinterpret_cast<Il2CppObject*>(array));
    if (!cls || a.class_get_rank(cls) != 1) return false;
    Il2CppClass* element = a.class_get_element_class(cls);
    if (!element || a.class_is_valuetype(element) == references) return false;
    *result = array;
    return true;
}
Il2CppObject* at(Il2CppArray* arr, int index) {
    if (!arr || index < 0 || static_cast<uintptr_t>(index) >= arr->length)
        return nullptr;
    return static_cast<Il2CppObject**>(arrayData(arr))[index];
}
bool actionPointer(Il2CppObject* owner, const char* field, int32_t** result) {
    auto& a = il2cpp::api();
    if (!owner || !a.object_get_class || !a.field_get_type ||
        !a.type_get_name || !a.il2cpp_free) return false;
    auto* cls = a.object_get_class(owner);
    size_t pos = 0;
    if (!offset(cls, field, sizeof(int32_t), false, &pos)) return false;
    FieldInfo* memberField = member(cls, field);
    Il2CppClass* type = a.class_from_il2cpp_type(a.field_get_type(memberField));
    if (!type || !a.class_value_size) return false;
    uint32_t align = 0;
    if (a.class_value_size(type, &align) != 4) return false;
    *result = reinterpret_cast<int32_t*>(static_cast<char*>(static_cast<void*>(owner)) + pos);
    return true;
}
bool enumerateProfile(Il2CppObject* profile,
                      std::vector<NativeLayoutBinding>* output) {
    auto& a = il2cpp::api();
    if (!profile || !a.object_get_class) return false;
    std::string id;
    if (!fieldString(profile, "Id", &id)) return false;
    Il2CppArray* controls = nullptr;
    if (!arrayField(profile, "Controls", &controls, true)) return false;
    for (int i = 0; i < static_cast<int>(controls->length); ++i) {
        Il2CppObject* binding = at(controls, i);
        if (!binding) continue;
        NativeLayoutBinding row;
        row.profile = id;
        row.index = i;
        if (!fieldString(binding, "SlotId", &row.slot))
            row.slot = "#" + std::to_string(i);
        if (!actionPointer(binding, "Action", &row.action)) return false;
        Il2CppArray* groups = nullptr;
        if (arrayField(binding, "GroupActions", &groups, false)) {
            if (groups->length > 16) return false;
            Il2CppClass* arrCls = a.object_get_class(
                reinterpret_cast<Il2CppObject*>(groups));
            Il2CppClass* element = a.class_get_element_class(arrCls);
            uint32_t align = 0;
            if (!element || !a.class_value_size ||
                a.class_value_size(element, &align) != 4) return false;
            auto* cells = static_cast<int32_t*>(arrayData(groups));
            for (size_t j = 0; j < groups->length; ++j)
                row.group.push_back(cells + j);
        }
        output->push_back(std::move(row));
    }
    return true;
}
Il2CppObject* instance(Il2CppClass* cls) {
    auto& a = il2cpp::api();
    if (!cls || !a.field_static_get_value) return nullptr;
    FieldInfo* field = member(cls, "Instance");
    if (field && (a.field_get_flags(field) & 0x10u)) {
        Il2CppObject* result = nullptr;
        a.field_static_get_value(field, &result);
        return result;
    }
    if (!a.class_get_method_from_name || !a.runtime_invoke ||
        !a.method_is_instance) return nullptr;
    const MethodInfo* getter = a.class_get_method_from_name(cls, "get_Instance", 0);
    if (!getter || a.method_is_instance(getter)) return nullptr;
    Il2CppObject* exception = nullptr;
    auto* result = a.runtime_invoke(getter, nullptr, nullptr, &exception);
    return exception ? nullptr : result;
}
}
Il2CppObject* currentNativeTouchStyles() {
    return instance(il2cpp::findClassQuiet("", "VirtualControlsStyles_Layout"));
}
bool enumerateNativeTouchShareProfile(Il2CppObject* profile,
                                     std::vector<NativeLayoutBinding>* output) {
    if (!profile || !output) return false;
    std::vector<NativeLayoutBinding> rows;
    if (!enumerateProfile(profile, &rows)) return false;
    *output = std::move(rows);
    return true;
}
bool enumerateNativeTouchLayouts(Il2CppObject* styles,
                                 std::vector<NativeLayoutBinding>* output) {
    if (!styles || !output) return false;
    std::vector<NativeLayoutBinding> rows;
    Il2CppArray* profiles = nullptr;
    if (!arrayField(styles, "Profiles", &profiles, true)) return false;
    std::set<Il2CppObject*> unique;
    for (size_t i = 0; i < profiles->length; ++i) {
        Il2CppObject* profile = at(profiles, static_cast<int>(i));
        if (!profile || !unique.insert(profile).second) continue;
        if (!enumerateProfile(profile, &rows)) return false;
    }
    // O perfil ativo pode existir fora da lista serializada.
    Il2CppObject* active = nullptr;
    auto& a = il2cpp::api();
    size_t off = 0;
    if (a.object_get_class &&
        (offset(a.object_get_class(styles), "_activeProfile",
                sizeof(void*), true, &off) ||
         offset(a.object_get_class(styles), "ActiveProfile",
                sizeof(void*), true, &off)))
        active = read<Il2CppObject*>(styles, off);
    if (active && unique.insert(active).second &&
        !enumerateProfile(active, &rows)) return false;
    *output = std::move(rows);
    return true;
}
std::string nativeTouchSaveDirectory() {
    auto& a = il2cpp::api();
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!main || !a.field_static_get_value ||
        !a.field_get_flags || !a.object_get_class)
        return {};
    Il2CppString* path = nullptr;
    FieldInfo* field = il2cpp::findField(main, "SavePath");
    if (field && (a.field_get_flags(field) & 0x10u))
        a.field_static_get_value(field, &path);
    else if (a.class_get_method_from_name && a.runtime_invoke &&
             a.method_is_instance) {
        const MethodInfo* getter = a.class_get_method_from_name(
            main, "get_SavePath", 0);
        if (!getter || a.method_is_instance(getter)) return {};
        Il2CppObject* ex = nullptr;
        path = reinterpret_cast<Il2CppString*>(
            a.runtime_invoke(getter, nullptr, nullptr, &ex));
        if (ex) return {};
    }
    // SavePath é um caminho interno Android; converter ASCII sem truncá-lo.
    std::string result;
    if (!stringValue(path, &result, 2048)) return {};
    if (result.find("..") != std::string::npos || result[0] != '/') return {};
    return result;
}
} // namespace bl::csharp::touch
