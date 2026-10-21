#include "experiments/csharp/NativeTouchStaticMappings.h"
#include "experiments/csharp/NativeTouchActionIds.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace bl::csharp::touch {
namespace {
struct Table {
    const char* cls;
    const char* field;
    int kind;
};
constexpr std::array<Table, 5> tables{{
    {"VirtualControlsButtonConfiguration_Layout", "ButtonStyles", 0},
    {"GUIVirtualInputController", "QuickActionControlFromLayoutControl", 1},
    {"GUIVirtualInputController", "ButtonControlFromLayoutControl", 2},
    {"VirtualControllerInputState", "DefaultMode", 3},
    {"VirtualControllerInputState", "RemappedMode", 4}
}};

bool initialized = false;
int failures = 0;

struct Prepared {
    FieldInfo* field = nullptr;
    Il2CppArray* existing = nullptr;
    uint32_t root = 0;
    int kind = -1;
};
bool elementIsInt32(Il2CppClass* element) {
    auto& api = il2cpp::api();
    if (!element || !api.class_get_name || !api.class_value_size ||
        !api.class_is_enum || !api.class_enum_basetype ||
        !api.type_get_name || !api.il2cpp_free) return false;
    uint32_t align = 0;
    if (api.class_value_size(element, &align) != 4) return false;
    if (!api.class_is_enum(element))
        return std::strcmp(api.class_get_name(element), "Int32") == 0;
    char* type = api.type_get_name(api.class_enum_basetype(element));
    bool valid = type && std::strcmp(type, "System.Int32") == 0;
    if (type) api.il2cpp_free(type);
    return valid;
}

int defaultValue(int kind, int index, int unbound) {
    if (kind == 1) return unbound;
    if (kind == 3 || kind == 4) return index;
    return 0;
}
int expectedValue(int kind, int id, int rawAction) {
    if (kind == 1) return nativeTouchQuickOf(id);
    if (kind == 2) return nativeTouchControlOf(id);
    if (kind == 3 || kind == 4) return rawAction;
    return 0;
}

bool inspect(const Table& spec, Prepared* output) {
    auto& api = il2cpp::api();
    auto* cls = il2cpp::findClassQuiet("", spec.cls);
    if (!cls) return false;
    FieldInfo* field = il2cpp::findField(cls, spec.field);
    if (!field || !(api.field_get_flags(field) & 0x10u)) return false;
    const Il2CppType* type = api.field_get_type(field);
    Il2CppClass* arrayType = type ? api.class_from_il2cpp_type(type) : nullptr;
    if (!arrayType || api.class_get_rank(arrayType) != 1) return false;
    Il2CppClass* element = api.class_get_element_class(arrayType);
    if (!elementIsInt32(element)) return false;

    Il2CppArray* array = nullptr;
    api.field_static_get_value(field, &array);
    if (!array || array->length > kMaxGameActionId ||
        !api.object_get_class ||
        api.object_get_class(reinterpret_cast<Il2CppObject*>(array)) != arrayType)
        return false;
    output->field = field;
    output->existing = array;
    output->kind = spec.kind;
    return true;
}

bool compileArray(Prepared* item, int required, int unbound) {
    auto& api = il2cpp::api();
    auto* source = item->existing;
    const auto* original = static_cast<const int32_t*>(arrayData(source));
    const int firstAction = nativeTouchActionOf(0);
    const int lastAction = nativeTouchActionOf(kMaxButtons - 1);
    if (firstAction < 0 || lastAction < firstAction ||
        required <= lastAction) return false;

    // Verifica toda a faixa reservada: se outro loader a ocupou, falha sem
    // alterar a memória vanilla. O valor 'default' é a única célula livre.
    bool needsWrite = source->length < static_cast<uintptr_t>(required);
    for (int id = 0; id < kMaxButtons; ++id) {
        int action = nativeTouchActionOf(id);
        int expected = expectedValue(item->kind, id, action);
        int fallback = defaultValue(item->kind, action, unbound);
        if (action < 0 || expected < 0) return false;
        if (static_cast<uintptr_t>(action) < source->length) {
            int value = original[action];
            if (value != expected && value != fallback) return false;
            if (value != expected) needsWrite = true;
        } else {
            needsWrite = true;
        }
    }
    if (!needsWrite) return true;

    Il2CppClass* arrayType = api.object_get_class(
        reinterpret_cast<Il2CppObject*>(source));
    auto* element = api.class_get_element_class(arrayType);
    auto* resized = api.array_new(element, static_cast<uintptr_t>(required));
    if (!resized || resized->length != static_cast<uintptr_t>(required))
        return false;
    item->root = api.gchandle_new(
        reinterpret_cast<Il2CppObject*>(resized), false);
    if (!item->root) return false;

    auto* data = static_cast<int32_t*>(arrayData(resized));
    const int previous = static_cast<int>(source->length);
    std::memcpy(data, original, static_cast<size_t>(previous) * sizeof(int32_t));
    for (int i = previous; i < required; ++i)
        data[i] = defaultValue(item->kind, i, unbound);
    for (int id = 0; id < kMaxButtons; ++id) {
        int action = nativeTouchActionOf(id);
        data[action] = expectedValue(item->kind, id, action);
    }
    return true;
}
}

void resetNativeTouchStaticMappingSession() {
    // Não limpar arrays vanilla no unload: os layouts do jogo são
    // proprietários deles e podem manter os IDs de mods desativados.
    initialized = false;
    failures = 0;
}

bool ensureNativeTouchStaticMappingsOnGameThread() {
    if (!nativeTouchObserverInstalled() || registry().count() == 0) return false;
    if (++failures > 4 && !initialized) return false;
    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.field_static_set_value ||
        !api.field_get_flags || !api.field_get_type ||
        !api.class_from_il2cpp_type || !api.class_get_rank ||
        !api.class_get_element_class || !api.array_new ||
        !api.gchandle_new || !api.gchandle_get_target ||
        !api.gchandle_free || !api.object_get_class) return false;

    std::array<Prepared, 5> prepared{};
    const auto release = [&] {
        for (auto& item : prepared)
            if (item.root) {
                api.gchandle_free(item.root);
                item.root = 0;
            }
    };
    int unbound = -1;
    // O unbound vem do enum, não deve ser deduzido de um número fixo.
    // Um id de QuickActionControl não relacionado nunca serve de fallback.
    // Referência: o default de QuickActionControlFromLayoutControl é Unbound.
    Il2CppClass* quick = il2cpp::findClassQuiet("", "QuickActionButton_Layout");
    Il2CppClass* enumType = quick ? il2cpp::findNested(quick, "QuickActionControl") : nullptr;
    FieldInfo* field = enumType ? il2cpp::findField(enumType, "Unbound") : nullptr;
    if (!field || !api.field_get_flags || !(api.field_get_flags(field) & 0x10u))
        return false;
    api.field_static_get_value(field, &unbound);
    if (unbound < 0 || unbound > 512) return false;

    const int required = nativeTouchActionOf(kMaxButtons - 1) + 1;
    if (required <= 0 || required > kMaxGameActionId) return false;

    for (size_t i = 0; i < tables.size(); ++i) {
        if (!inspect(tables[i], &prepared[i]) ||
            !compileArray(&prepared[i], required, unbound)) {
            release();
            return false;
        }
    }

    // A publicação só começa após as cinco tabelas verificarem dimensões,
    // tipos e células de conflito. Todos os arrays novos estão enraizados.
    for (auto& item : prepared) {
        if (!item.root) continue;
        Il2CppObject* array = api.gchandle_get_target(item.root);
        if (!array) {
            release();
            return false;
        }
        api.field_static_set_value(item.field, &array);
    }
    release();
    initialized = true;
    failures = 0;
    return true;
}
} // namespace bl::csharp::touch
