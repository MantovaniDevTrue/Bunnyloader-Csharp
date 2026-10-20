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
    size_t pressed = 0;
    FieldInfo* gameMenu = nullptr;
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

// O método possui apenas (float), cuja ABI AArch64 é estável. Nunca
// chamamos a assinatura HandleTouchCursorDown sem confirmar seu Vector2.
void onControllerUpdate(void* self, float elapsedTime, const MethodInfo* method) {
    UpdateFn original = __atomic_load_n(&originalUpdate, __ATOMIC_ACQUIRE);
    if (original) original(self, elapsedTime, method);
    if (!self || !installed.load(std::memory_order_acquire) ||
        registry().count() == 0) return;

    auto& api = il2cpp::api();
    bool menu = true;
    api.field_static_get_value(refs.gameMenu, &menu);
    if (menu) {
        registry().frame(static_cast<int64_t>(++ticks), false, true,
                         true, true, nullptr, 0, nullptr, 0);
        return;
    }
    Il2CppArray* pressed = nullptr;
    std::memcpy(&pressed, static_cast<const char*>(self) + refs.pressed,
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
    FieldInfo* pressed = controller ? il2cpp::findField(controller, "Pressed") : nullptr;
    if (!update || !pressed || !menuField(&refs.gameMenu) ||
        !enumCount(controller, "ControlType", &refs.controlBase)) return;
    const size_t offset = api.field_get_offset(pressed);
    if (offset < sizeof(Il2CppObject) ||
        offset + sizeof(void*) > api.class_instance_size(controller)) return;
    refs.pressed = offset;
    if (!bl::hook::install(update, onControllerUpdate, &originalUpdate) ||
        !originalUpdate) return;
    installed.store(true, std::memory_order_release);
    // Importante: NÃO chamar setHostReady(true) aqui. O editor e o
    // salvamento ainda não foram conectados.
}

} // namespace bl::csharp::touch
