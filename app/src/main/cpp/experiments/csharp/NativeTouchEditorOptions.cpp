#include "experiments/csharp/NativeTouchEditorOptions.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchEditor.h"
#include "experiments/csharp/NativeTouchStaticMappings.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bl::csharp::touch {
namespace {

FieldInfo* member(Il2CppClass* cls, const char* name) {
    FieldInfo* field = cls ? il2cpp::findField(cls, name) : nullptr;
    if (field) return field;
    // Campos autoimplementados de propriedades C#, se a ABI os expuser.
    const std::string alternate = std::string("<") + name + ">k__BackingField";
    return cls ? il2cpp::findField(cls, alternate.c_str()) : nullptr;
}

bool intField(Il2CppClass* cls, const char* name, size_t* offset) {
    auto& api = il2cpp::api();
    FieldInfo* field = member(cls, name);
    if (!field || !api.field_get_flags || (api.field_get_flags(field) & 0x10u) ||
        !api.field_get_type || !api.type_get_name || !api.il2cpp_free ||
        !api.field_get_offset || !api.class_instance_size) return false;
    char* type = api.type_get_name(api.field_get_type(field));
    const bool valid = type && std::strcmp(type, "System.Int32") == 0;
    if (type) api.il2cpp_free(type);
    if (!valid) return false;
    const size_t position = api.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + sizeof(int32_t) > api.class_instance_size(cls)) return false;
    *offset = position;
    return true;
}

bool boolField(Il2CppClass* cls, const char* name, size_t* offset) {
    auto& api = il2cpp::api();
    FieldInfo* field = member(cls, name);
    if (!field || !api.field_get_flags || (api.field_get_flags(field) & 0x10u) ||
        !api.field_get_type || !api.type_get_name || !api.il2cpp_free ||
        !api.field_get_offset || !api.class_instance_size) return false;
    char* type = api.type_get_name(api.field_get_type(field));
    const bool valid = type && std::strcmp(type, "System.Boolean") == 0;
    if (type) api.il2cpp_free(type);
    if (!valid) return false;
    const size_t position = api.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + sizeof(uint8_t) > api.class_instance_size(cls)) return false;
    *offset = position;
    return true;
}

bool refField(Il2CppClass* cls, const char* name, size_t* offset) {
    auto& api = il2cpp::api();
    FieldInfo* field = member(cls, name);
    if (!field || !api.field_get_flags || (api.field_get_flags(field) & 0x10u) ||
        !api.field_get_type || !api.class_from_il2cpp_type ||
        !api.class_is_valuetype || !api.field_get_offset ||
        !api.class_instance_size) return false;
    Il2CppClass* type = api.class_from_il2cpp_type(api.field_get_type(field));
    if (!type || api.class_is_valuetype(type)) return false;
    const size_t position = api.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + sizeof(void*) > api.class_instance_size(cls)) return false;
    *offset = position;
    return true;
}

template<class T> T read(const void* object, size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const char*>(object) + offset, sizeof(value));
    return value;
}
template<class T> void write(void* object, size_t offset, T value) {
    std::memcpy(static_cast<char*>(object) + offset, &value, sizeof(value));
}

Il2CppObject* configurationInstance(Il2CppClass* cls) {
    auto& api = il2cpp::api();
    FieldInfo* field = member(cls, "Instance");
    if (field && api.field_get_flags &&
        (api.field_get_flags(field) & 0x10u)) {
        Il2CppObject* value = nullptr;
        api.field_static_get_value(field, &value);
        return value;
    }
    if (!api.class_get_method_from_name || !api.method_is_instance ||
        !api.runtime_invoke) return nullptr;
    auto* getter = api.class_get_method_from_name(cls, "get_Instance", 0);
    if (!getter || api.method_is_instance(getter)) return nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* value = api.runtime_invoke(getter, nullptr, nullptr, &error);
    return error ? nullptr : value;
}

bool selectorReady(Il2CppObject** config, Il2CppObject** grid, Il2CppArray** list,
                   size_t* buttons, size_t* itemCount, size_t* lineCount,
                   size_t* horizontal) {
    auto& api = il2cpp::api();
    Il2CppClass* cls = il2cpp::findClassQuiet(
        "", "VirtualInputControllerControlConfiguration_Layout");
    if (!cls || !api.object_get_class || !api.class_is_assignable_from)
        return false;
    *config = configurationInstance(cls);
    if (!*config || !api.class_is_assignable_from(
            cls, api.object_get_class(*config))) return false;

    size_t gridOffset = 0;
    if (!refField(cls, "ButtonControls", buttons) ||
        !refField(cls, "PulldownOptionGrid", &gridOffset)) return false;
    *list = read<Il2CppArray*>(*config, *buttons);
    *grid = read<Il2CppObject*>(*config, gridOffset);
    if (!*list || !*grid || (*list)->length > 1024) return false;

    Il2CppClass* listType = api.object_get_class(
        reinterpret_cast<Il2CppObject*>(*list));
    if (!listType || !api.class_get_rank || api.class_get_rank(listType) != 1 ||
        !api.class_get_element_class) return false;
    Il2CppClass* element = api.class_get_element_class(listType);
    if (!element || !api.class_is_enum || !api.class_is_enum(element) ||
        !api.class_get_name ||
        std::strcmp(api.class_get_name(element), "ContolType") != 0 ||
        !api.class_value_size) return false;
    uint32_t align = 0;
    if (api.class_value_size(element, &align) != 4) return false;

    Il2CppClass* gridType = api.object_get_class(*grid);
    return intField(gridType, "ItemCount", itemCount) &&
           intField(gridType, "ItemLineCount", lineCount) &&
           boolField(gridType, "Horizontal", horizontal);
}
}

bool ensureNativeTouchEditorOptionsOnGameThread() {
    if (!nativeTouchEditorLabelsInstalled() || registry().count() == 0 ||
        !ensureNativeTouchStaticMappingsOnGameThread()) return false;
    auto& api = il2cpp::api();
    if (!api.array_new || !api.gchandle_new || !api.gchandle_free ||
        !api.gchandle_get_target || !api.gc_wbarrier_set_field)
        return false;

    Il2CppObject* config = nullptr;
    Il2CppObject* grid = nullptr;
    Il2CppArray* list = nullptr;
    size_t buttonsOffset = 0, itemOffset = 0, lineOffset = 0, horizontalOffset = 0;
    if (!selectorReady(&config, &grid, &list, &buttonsOffset,
                       &itemOffset, &lineOffset, &horizontalOffset)) return false;

    std::vector<int32_t> items;
    items.reserve(static_cast<size_t>(list->length) + kMaxButtons);
    const int32_t* original = static_cast<const int32_t*>(arrayData(list));
    // Quando um mod é descarregado, seus IDs saem da lista de opções.
    for (size_t i = 0; i < list->length; ++i) {
        int id = nativeTouchIndexByAction(original[i]);
        if (id >= 0 && registry().displayName(id).empty()) continue;
        items.push_back(original[i]);
    }
    for (int id = 0; id < kMaxButtons; ++id) {
        if (registry().displayName(id).empty()) continue;
        int32_t action = nativeTouchActionOf(id);
        if (action < 0) return false;
        if (std::find(items.begin(), items.end(), action) == items.end())
            items.push_back(action);
    }
    if (items.size() > 1024) return false;

    const int oldItems = read<int32_t>(grid, itemOffset);
    const int oldLines = read<int32_t>(grid, lineOffset);
    const bool wide = read<uint8_t>(grid, horizontalOffset) != 0;
    if (oldItems <= 0 || oldLines <= 0 || oldItems > 1024 ||
        oldLines > 1024) return false;

    bool changed = items.size() != list->length;
    if (!changed)
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i] != original[i]) { changed = true; break; }
    if (!changed) return true;

    Il2CppClass* listType = api.object_get_class(
        reinterpret_cast<Il2CppObject*>(list));
    Il2CppClass* element = api.class_get_element_class(listType);
    auto* replacement = api.array_new(element, items.size());
    if (!replacement || replacement->length != items.size()) return false;
    uint32_t root = api.gchandle_new(
        reinterpret_cast<Il2CppObject*>(replacement), false);
    if (!root) return false;
    if (!items.empty())
        std::memcpy(arrayData(replacement), items.data(),
                    items.size() * sizeof(int32_t));

    int newItems = oldItems;
    int newLines = oldLines;
    if (static_cast<int>(items.size()) > newItems) {
        if (wide) {
            newItems = ((static_cast<int>(items.size()) + newLines - 1) /
                         newLines) * newLines;
        } else {
            int columns = std::max(1, (newItems + newLines - 1) / newLines);
            newLines = (static_cast<int>(items.size()) + columns - 1) / columns;
            newItems = newLines * columns;
        }
    }
    auto* value = api.gchandle_get_target(root);
    if (!value) {
        api.gchandle_free(root);
        return false;
    }
    auto* destination = reinterpret_cast<void**>(
        static_cast<char*>(static_cast<void*>(config)) + buttonsOffset);
    api.gc_wbarrier_set_field(config, destination, value);
    api.gchandle_free(root);
    write<int32_t>(grid, itemOffset, newItems);
    write<int32_t>(grid, lineOffset, newLines);
    return true;
}
} // namespace bl::csharp::touch
