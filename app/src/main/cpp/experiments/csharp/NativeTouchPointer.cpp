#include "experiments/csharp/NativeTouchPointer.h"
#include "experiments/csharp/NativeTouchProfiles.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::touch {
namespace {
struct Vec2 { float x, y; };
static_assert(sizeof(Vec2) == 8);
// Vector2 é HFA de dois floats na ABI ARM64; Cursor precisa ser referência.
// Não é seguro substituir isso por 8 bytes em registradores inteiros.
using TouchFn = void (*)(void*, int32_t, Vec2, Il2CppObject*, const MethodInfo*);
TouchFn originalTouch = nullptr;
std::atomic<bool> hooked{false};
bool tried = false;

size_t pressedOffset = 0;
size_t wasOffset = 0;

bool arrayField(Il2CppClass* controller, const char* name, size_t* offset) {
    auto& a = il2cpp::api();
    FieldInfo* field = controller ? il2cpp::findField(controller, name) : nullptr;
    if (!field || !a.field_get_flags || !a.field_get_offset ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_rank || !a.class_get_element_class ||
        !a.class_get_name || !a.class_instance_size ||
        (a.field_get_flags(field) & 0x10u)) return false;
    Il2CppClass* array = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!array || a.class_get_rank(array) != 1) return false;
    Il2CppClass* element = a.class_get_element_class(array);
    if (!element || !a.class_get_name(element) ||
        std::strcmp(a.class_get_name(element), "Boolean") != 0) return false;
    size_t raw = a.field_get_offset(field);
    if (raw < sizeof(Il2CppObject) ||
        raw + sizeof(void*) > a.class_instance_size(controller)) return false;
    *offset = raw;
    return true;
}

bool nativeTouchShape(const MethodInfo* method) {
    auto& a = il2cpp::api();
    if (!method || !a.method_get_param_count || !a.method_is_instance ||
        !a.class_from_il2cpp_type || !a.class_is_valuetype ||
        !a.class_get_name || !a.method_get_param ||
        !a.method_get_return_type || !a.type_get_name || !a.il2cpp_free ||
        !a.method_is_instance(method) ||
        a.method_get_param_count(method) != 3 ||
        !il2cpp::methodPointer(method)) return false;
    Il2CppClass* slot = a.class_from_il2cpp_type(a.method_get_param(method, 0));
    Il2CppClass* vector = a.class_from_il2cpp_type(a.method_get_param(method, 1));
    Il2CppClass* cursor = a.class_from_il2cpp_type(a.method_get_param(method, 2));
    if (!slot || !vector || !cursor ||
        std::strcmp(a.class_get_name(slot), "Int32") != 0 ||
        std::strcmp(a.class_get_name(vector), "Vector2") != 0 ||
        std::strcmp(a.class_get_name(cursor), "Cursor") != 0 ||
        !a.class_is_valuetype(vector) || a.class_is_valuetype(cursor))
        return false;
    il2cpp::ValueLayout layout{};
    std::string error;
    if (!il2cpp::resolveValueLayout(vector, &layout, &error) ||
        layout.size != sizeof(Vec2)) return false;
    constexpr const char* names[] = {"X", "Y"};
    for (int i = 0; i < 2; ++i) {
        FieldInfo* field = il2cpp::findField(vector, names[i]);
        if (!field || !a.field_get_type || !a.field_get_offset) return false;
        char* type = a.type_get_name(a.field_get_type(field));
        bool valid = type && std::strcmp(type, "System.Single") == 0 &&
                     a.field_get_offset(field) == layout.header + static_cast<size_t>(i * 4);
        if (type) a.il2cpp_free(type);
        if (!valid) return false;
    }
    char* ret = a.type_get_name(a.method_get_return_type(method));
    bool voidReturn = ret && std::strcmp(ret, "System.Void") == 0;
    if (ret) a.il2cpp_free(ret);
    return voidReturn;
}
Il2CppArray* inputArray(void* controller, size_t offset) {
    Il2CppArray* array = nullptr;
    std::memcpy(&array, static_cast<char*>(controller) + offset, sizeof(array));
    return array && array->length <= 4096 ? array : nullptr;
}
void onTouch(void* controller, int32_t slot, Vec2 position, Il2CppObject* cursor,
             const MethodInfo* method) {
    TouchFn original = __atomic_load_n(&originalTouch, __ATOMIC_ACQUIRE);
    if (!hooked.load(std::memory_order_acquire) || slot < 0 ||
        !std::isfinite(position.x) || !std::isfinite(position.y)) {
        if (original) original(controller, slot, position, cursor, method);
        return;
    }
    SlotAction action;
    if (!nativeTouchSlot(slot, &action) || action.style != 1) {
        if (original) original(controller, slot, position, cursor, method);
        return;
    }
    const int id = nativeTouchIndexByAction(action.action);
    if (id < 0 || id >= kMaxButtons) {
        if (original) original(controller, slot, position, cursor, method);
        return;
    }
    if (!hostReady() || registry().ownerId(id).empty() ||
        !registry().visible(id) || !registry().enabled(id))
        return;
    const int control = nativeTouchControlOf(id);
    Il2CppArray* pressed = inputArray(controller, pressedOffset);
    Il2CppArray* wasPressed = inputArray(controller, wasOffset);
    if (control < 0 || !pressed || !wasPressed ||
        static_cast<uintptr_t>(control) >= pressed->length ||
        static_cast<uintptr_t>(control) >= wasPressed->length)
        return;
    auto* current = static_cast<uint8_t*>(arrayData(pressed));
    auto* old = static_cast<const uint8_t*>(arrayData(wasPressed));
    current[control] = 1;
    if (old[control] == 0) registry().tap(id);
}

} // namespace

bool nativeTouchPointerInstalled() {
    return hooked.load(std::memory_order_acquire);
}
void pumpNativeTouchPointerHookOnGameThread() {
    if (hooked.load(std::memory_order_acquire) || tried ||
        !nativeTouchObserverInstalled()) return;
#if !defined(__aarch64__)
    return;
#else
    static uint32_t probes = 0;
    if (++probes != 1 && probes % 120 != 0) return;
    Il2CppClass* controller = il2cpp::findClassQuiet("", "GUIVirtualInputController");
    const MethodInfo* method = controller ? il2cpp::findMethodBySignature(
        controller, il2cpp::parseSignature(
            "void HandleTouchCursorDown(int slot, Vector2 cursorUIPosition, Cursor cursor)"))
        : nullptr;
    if (!nativeTouchShape(method) ||
        !arrayField(controller, "Pressed", &pressedOffset) ||
        !arrayField(controller, "WasPressed", &wasOffset)) return;
    tried = true;
    if (!bl::hook::install(method, onTouch, &originalTouch) || !originalTouch) return;
    hooked.store(true, std::memory_order_release);
#endif
}
} // namespace bl::csharp::touch
