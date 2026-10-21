#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "experiments/csharp/NativeTouchGameAbi.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace bl::csharp::touch {
namespace {
using UpdateFn = void (*)(void*, float, const MethodInfo*);
UpdateFn originalUpdate = nullptr;
std::atomic<bool> installed{false};
bool attempted = false;

struct RuntimeFields {
    std::array<size_t, 3> offsets{};
    FieldInfo* gameMenu = nullptr;
    Il2CppClass* controller = nullptr;
    int controlBase = -1;
};
RuntimeFields refs;
uint64_t ticks = 0;

bool enumCount(Il2CppClass* owner, const char* nested, int* value) {
    auto& api = il2cpp::api();
    if (!owner || !api.field_static_get_value || !api.field_get_flags ||
        !api.field_get_type || !api.class_from_il2cpp_type ||
        !api.class_is_enum || !api.class_enum_basetype ||
        !api.type_get_name || !api.il2cpp_free)
        return false;
    Il2CppClass* type = il2cpp::findNested(owner, nested);
    FieldInfo* field = type ? il2cpp::findField(type, "Count") : nullptr;
    if (!field || !(api.field_get_flags(field) & 0x10u)) return false;
    const Il2CppType* fieldType = api.field_get_type(field);
    Il2CppClass* enumType = fieldType ? api.class_from_il2cpp_type(fieldType) : nullptr;
    if (!enumType || !api.class_is_enum(enumType)) return false;
    char* name = api.type_get_name(api.class_enum_basetype(enumType));
    const bool validType = name && (std::strcmp(name, "System.Int32") == 0 ||
                                    std::strcmp(name, "System.UInt32") == 0);
    if (name) api.il2cpp_free(name);
    if (!validType) return false;
    int count = -1;
    api.field_static_get_value(field, &count);
    if (count <= 0 || count > 256) return false;
    *value = count;
    return true;
}

bool menuField(FieldInfo** output) {
    auto& api = il2cpp::api();
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    FieldInfo* field = main ? il2cpp::findField(main, "gameMenu") : nullptr;
    if (!field || !api.field_get_flags || !api.field_get_type ||
        !api.type_get_name || !api.il2cpp_free ||
        !(api.field_get_flags(field) & 0x10u)) return false;
    char* type = api.type_get_name(api.field_get_type(field));
    const bool valid = type && std::strcmp(type, "System.Boolean") == 0;
    if (type) api.il2cpp_free(type);
    if (valid) *output = field;
    return valid;
}

// Cresce os três arrays em conjunto, sem apagar os estados vanilla.
// Nunca grava um array novo sem a barreira do GC incremental do IL2CPP.
// Só é chamado na thread do jogo, quando existem ações C# nativas.
bool ensureControllerArrays(void* self) {
    auto& api = il2cpp::api();
    if (!api.array_new || !api.object_get_class ||
        !api.class_get_element_class || !api.gc_wbarrier_set_field ||
        !api.class_is_assignable_from || !refs.controller ||
        !api.class_is_assignable_from(refs.controller,
            api.object_get_class(reinterpret_cast<Il2CppObject*>(self))))
        return false;

    const int required = refs.controlBase + kMaxButtons;
    if (required <= 0 || required > 512) return false;
    std::array<Il2CppArray*, 3> arrays{};
    std::array<Il2CppArray*, 3> next{};
    constexpr std::array<size_t, 3> widths{1, 1, 4};
    constexpr std::array<const char*, 3> typeNames{"Boolean", "Boolean", "Single"};

    for (size_t i = 0; i < arrays.size(); ++i) {
        std::memcpy(&arrays[i], static_cast<const char*>(self) + refs.offsets[i],
                    sizeof(Il2CppArray*));
        if (!arrays[i] || arrays[i]->length > 4096) return false;
        Il2CppClass* arrayType =
            api.object_get_class(reinterpret_cast<Il2CppObject*>(arrays[i]));
        Il2CppClass* element = arrayType ?
            api.class_get_element_class(arrayType) : nullptr;
        if (!element || !api.class_get_name(element) ||
            std::strcmp(api.class_get_name(element), typeNames[i]) != 0)
            return false;
        if (arrays[i]->length >= static_cast<uintptr_t>(required)) continue;
        next[i] = api.array_new(element, static_cast<uintptr_t>(required));
        if (!next[i] || next[i]->length != static_cast<uintptr_t>(required))
            return false;
        std::memcpy(arrayData(next[i]), arrayData(arrays[i]),
                    static_cast<size_t>(arrays[i]->length) * widths[i]);
    }
    for (size_t i = 0; i < next.size(); ++i) {
        if (!next[i]) continue;
        auto* destination = reinterpret_cast<void**>(
            static_cast<char*>(self) + refs.offsets[i]);
        api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(self),
                                 destination, next[i]);
    }
    return true;
}

// O método possui apenas (float), cuja ABI AArch64 é estável. Nunca
// chamamos a assinatura HandleTouchCursorDown sem confirmar seu Vector2.
void onControllerUpdate(void* self, float elapsedTime, const MethodInfo* method) {
    UpdateFn original = __atomic_load_n(&originalUpdate, __ATOMIC_ACQUIRE);
    const bool observe = self && installed.load(std::memory_order_acquire) &&
                         registry().count() > 0;
    const bool arraysReady = observe && ensureControllerArrays(self);
    if (original) original(self, elapsedTime, method);
    if (!arraysReady) return;

    auto& api = il2cpp::api();
    bool menu = true;
    api.field_static_get_value(refs.gameMenu, &menu);
    if (menu) {
        registry().frame(static_cast<int64_t>(++ticks), false, true,
                         true, true, nullptr, 0, nullptr, 0);
        return;
    }
    Il2CppArray* pressed = nullptr;
    std::memcpy(&pressed, static_cast<const char*>(self) + refs.offsets[0],
                sizeof(pressed));
    if (!pressed || pressed->length > 2048) return;

    const uint8_t* data = static_cast<const uint8_t*>(arrayData(pressed));
    std::array<int, kMaxButtons> held{};
    size_t count = 0;
    // Os IDs do registry são locais ao boot; o offset do enum é descoberto
    // por reflexão, e nunca assume o Count fixo de outra versão do jogo.
    for (int id = 0; id < kMaxButtons; ++id) {
        const int index = refs.controlBase + id;
        if (index >= 0 && static_cast<uintptr_t>(index) < pressed->length &&
            data[index] != 0)
            held[count++] = id;
    }
    registry().frame(static_cast<int64_t>(++ticks), true, true, true, true,
                     held.data(), count, nullptr, 0);
}

} // namespace

bool nativeTouchObserverInstalled() {
    return installed.load(std::memory_order_acquire);
}

void pumpNativeTouchObserverOnGameThread() {
    if (installed.load(std::memory_order_acquire) || attempted) return;
    auto& api = il2cpp::api();
    if (!api.gameImage || !api.field_get_offset || !api.field_static_get_value ||
        !api.class_instance_size) return;

    // A tentativa é definitiva nesta sessão: ABI desconhecida não deve
    // disparar scans ou reinicializações continuamente durante o gameplay.
    attempted = true;
    HostAbiCheck check = inspectNativeTouchAbi();
    if (!check.controller || !check.arrays) return;
    Il2CppClass* controller = il2cpp::findClassQuiet("", "GUIVirtualInputController");
    const auto* update = controller ? il2cpp::findMethodBySignature(
        controller, il2cpp::parseSignature("void Update(float elapsedTime)")) : nullptr;
    if (!update || !menuField(&refs.gameMenu) ||
        !enumCount(controller, "ControlType", &refs.controlBase)) return;
    constexpr std::array<const char*, 3> fields{"Pressed", "WasPressed", "Scale"};
    for (size_t i = 0; i < fields.size(); ++i) {
        FieldInfo* field = controller ? il2cpp::findField(controller, fields[i]) : nullptr;
        if (!field) return;
        const size_t offset = api.field_get_offset(field);
        if (offset < sizeof(Il2CppObject) ||
            offset + sizeof(void*) > api.class_instance_size(controller)) return;
        refs.offsets[i] = offset;
    }
    refs.controller = controller;
    if (!bl::hook::install(update, onControllerUpdate, &originalUpdate) ||
        !originalUpdate) return;
    installed.store(true, std::memory_order_release);
    // Importante: NÃO chamar setHostReady(true) aqui. O editor e o
    // salvamento ainda não foram conectados.
}

} // namespace bl::csharp::touch
