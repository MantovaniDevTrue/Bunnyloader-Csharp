#include "experiments/csharp/NativeTouchFocus.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cstdint>
#include <cstring>

namespace bl::csharp::touch {
namespace {
FieldInfo* chatField = nullptr;
FieldInfo* playersField = nullptr;
FieldInfo* indexField = nullptr;
size_t npcOffset = 0;
size_t signOffset = 0;
bool attempted = false;
bool bound = false;

template<typename T> T load(const void* object, size_t offset) {
    T result{};
    std::memcpy(&result, static_cast<const char*>(object) + offset, sizeof(result));
    return result;
}
bool type(FieldInfo* f, const char* name) {
    auto& a = il2cpp::api();
    if (!f || !a.field_get_type || !a.type_get_name || !a.il2cpp_free)
        return false;
    char* value = a.type_get_name(a.field_get_type(f));
    bool result = value && std::strcmp(value, name) == 0;
    if (value) a.il2cpp_free(value);
    return result;
}
bool instanceInt(Il2CppClass* cls, const char* name, size_t* out) {
    auto& a = il2cpp::api();
    FieldInfo* field = cls ? il2cpp::findField(cls, name) : nullptr;
    if (!field || !type(field, "System.Int32") ||
        !a.field_get_flags || (a.field_get_flags(field) & 0x10u) ||
        !a.field_get_offset || !a.class_instance_size) return false;
    size_t position = a.field_get_offset(field);
    if (position < sizeof(Il2CppObject) ||
        position + sizeof(int32_t) > a.class_instance_size(cls)) return false;
    *out = position;
    return true;
}
bool prepare() {
    auto& a = il2cpp::api();
    if (!a.field_get_flags || !a.field_static_get_value ||
        !a.object_get_class || !a.class_get_rank ||
        !a.class_get_element_class || !a.class_get_name) return false;
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* player = il2cpp::findClass({"Terraria", "Player", {}});
    chatField = main ? il2cpp::findField(main, "drawingPlayerChat") : nullptr;
    playersField = main ? il2cpp::findField(main, "player") : nullptr;
    indexField = main ? il2cpp::findField(main, "myPlayer") : nullptr;
    if (!chatField || !playersField || !indexField ||
        !type(chatField, "System.Boolean") ||
        !type(indexField, "System.Int32") ||
        !(a.field_get_flags(chatField) & 0x10u) ||
        !(a.field_get_flags(playersField) & 0x10u) ||
        !(a.field_get_flags(indexField) & 0x10u) ||
        !instanceInt(player, "talkNPC", &npcOffset) ||
        !instanceInt(player, "sign", &signOffset))
        return false;
    return true;
}
}

FocusState readNativeTouchFocus() {
    FocusState focus;
    if (!attempted) {
        attempted = true;
        bound = prepare();
    }
    if (!bound) return focus;
    auto& a = il2cpp::api();
    uint8_t chat = 1;
    int32_t index = -1;
    Il2CppArray* players = nullptr;
    a.field_static_get_value(chatField, &chat);
    a.field_static_get_value(indexField, &index);
    a.field_static_get_value(playersField, &players);
    if (!players || index < 0 ||
        static_cast<uintptr_t>(index) >= players->length ||
        players->length > 256) return focus;
    Il2CppClass* array = a.object_get_class(
        reinterpret_cast<Il2CppObject*>(players));
    if (!array || a.class_get_rank(array) != 1) return focus;
    Il2CppClass* element = a.class_get_element_class(array);
    if (!element || !a.class_get_name(element) ||
        std::strcmp(a.class_get_name(element), "Player") != 0) return focus;
    auto* values = static_cast<Il2CppObject**>(arrayData(players));
    Il2CppObject* player = values[index];
    if (!player || a.object_get_class(player) != element) return focus;
    focus.chat = chat != 0;
    focus.npc = load<int32_t>(player, npcOffset) != -1;
    focus.sign = load<int32_t>(player, signOffset) != -1;
    focus.valid = true;
    return focus;
}

} // namespace bl::csharp::touch
