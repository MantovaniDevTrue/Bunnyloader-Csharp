#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "experiments/csharp/NativeTouchGameAbi.h"
#include "experiments/csharp/NativeTouchActionIds.h"
#include "experiments/csharp/NativeTouchProfiles.h"
#include "experiments/csharp/NativeTouchFocus.h"
#include "experiments/csharp/NativeTouchGroups.h"
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
    std::array<Il2CppClass*, 3> arrayTypes{};
    FieldInfo* gameMenu = nullptr;
    Il2CppClass* controller = nullptr;
    ActionIds ids;
    int actionUnbound = -1;
    int quickUnbound = -1;
};
RuntimeFields refs;
uint64_t ticks = 0;

bool enumValue(Il2CppClass* owner, const char* nested, const char* member, int* value) {
    auto& api = il2cpp::api();
    if (!owner || !api.field_static_get_value || !api.field_get_flags ||
        !api.field_get_type || !api.class_from_il2cpp_type ||
        !api.class_is_enum || !api.class_enum_basetype ||
        !api.type_get_name || !api.il2cpp_free)
        return false;
    Il2CppClass* type = il2cpp::findNested(owner, nested);
    FieldInfo* field = type ? il2cpp::findField(type, member) : nullptr;
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
        !api.gchandle_new || !api.gchandle_get_target ||
        !api.gchandle_free || !api.class_is_assignable_from ||
        !refs.controller ||
        !api.class_is_assignable_from(refs.controller,
            api.object_get_class(reinterpret_cast<Il2CppObject*>(self))))
        return false;

    const int required = refs.ids.control(kMaxButtons - 1) + 1;
    if (!refs.ids.valid() || required <= 0 || required > kMaxGameActionId) return false;
    std::array<Il2CppArray*, 3> arrays{};
    std::array<uint32_t, 3> roots{};
    const auto release = [&] {
        for (uint32_t root : roots)
            if (root) api.gchandle_free(root);
    };
    constexpr std::array<size_t, 3> widths{1, 1, 4};

    for (size_t i = 0; i < arrays.size(); ++i) {
        std::memcpy(&arrays[i], static_cast<const char*>(self) + refs.offsets[i],
                    sizeof(Il2CppArray*));
        if (!arrays[i] || arrays[i]->length > 4096) {
            release();
            return false;
        }
        Il2CppClass* arrayType =
            api.object_get_class(reinterpret_cast<Il2CppObject*>(arrays[i]));
        if (!refs.arrayTypes[i] || arrayType != refs.arrayTypes[i]) {
            release();
            return false;
        }
        if (arrays[i]->length >= static_cast<uintptr_t>(required)) continue;
        Il2CppClass* element = api.class_get_element_class(refs.arrayTypes[i]);
        if (!element) { release(); return false; }
        Il2CppArray* resized = api.array_new(element, static_cast<uintptr_t>(required));
        if (!resized || resized->length != static_cast<uintptr_t>(required)) {
            release();
            return false;
        }
        roots[i] = api.gchandle_new(
            reinterpret_cast<Il2CppObject*>(resized), false);
        if (!roots[i]) {
            release();
            return false;
        }
        std::memcpy(arrayData(resized), arrayData(arrays[i]),
                    static_cast<size_t>(arrays[i]->length) * widths[i]);
    }
    for (size_t i = 0; i < roots.size(); ++i) {
        if (!roots[i]) continue;
        auto* value = api.gchandle_get_target(roots[i]);
        if (!value) {
            release();
            return false;
        }
        auto* destination = reinterpret_cast<void**>(
            static_cast<char*>(self) + refs.offsets[i]);
        api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(self),
                                 destination, value);
    }
    release();
    return true;
}

// O método possui apenas (float), cuja ABI AArch64 é estável. Nunca
// chamamos a assinatura HandleTouchCursorDown sem confirmar seu Vector2.
void onControllerUpdate(void* self, float elapsedTime, const MethodInfo* method) {
    UpdateFn original = __atomic_load_n(&originalUpdate, __ATOMIC_ACQUIRE);
    const bool observe = self && installed.load(std::memory_order_acquire) &&
                         registry().count() > 0;
    const bool arraysReady = observe && ensureControllerArrays(self);
    if (arraysReady) nativeTouchGroupBefore(self);
    if (original) original(self, elapsedTime, method);
    if (!arraysReady) return;
    nativeTouchGroupAfter(self);

    auto& api = il2cpp::api();
    bool menu = true;
    api.field_static_get_value(refs.gameMenu, &menu);
    static ActiveTouchSnapshot snapshot;
    static const void* scannedProfile = nullptr;
    static uint64_t previousScan = 0;
    static bool sampled = false;
    if (menu) {
        snapshot = {};
        scannedProfile = nullptr;
        previousScan = 0;
        sampled = false;
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
        const int index = refs.ids.control(id);
        if (index >= 0 && static_cast<uintptr_t>(index) < pressed->length &&
            data[index] != 0)
            held[count++] = id;
    }
    // Os layouts mudam durante a edição. A leitura da configuração ocorre
    // no máximo a cada 30 frames; o gameplay não escaneia perfis por toque.
    const void* currentProfile = nativeTouchActiveProfileIdentity();
    if (!sampled || scannedProfile != currentProfile ||
        ticks - previousScan >= 30) {
        ActiveTouchSnapshot next;
        if (currentProfile && nativeTouchActiveSnapshot(&next)) snapshot = next;
        else snapshot = {};
        scannedProfile = currentProfile;
        previousScan = ticks;
        sampled = true;
    }
    std::array<int, kMaxButtons> bound{};
    size_t boundCount = 0;
    if (snapshot.ready) {
        for (int id = 0; id < kMaxButtons; ++id)
            if (snapshot.bound[static_cast<size_t>(id)])
                bound[boundCount++] = id;
    }
    const FocusState focus = readNativeTouchFocus();
    registry().frame(static_cast<int64_t>(++ticks), true, focus.chat,
                     focus.npc, focus.sign, held.data(), count,
                     bound.data(), boundCount);
}

} // namespace

bool nativeTouchObserverInstalled() {
    return installed.load(std::memory_order_acquire);
}
int nativeTouchActionOf(int id) {
    return nativeTouchObserverInstalled() ? refs.ids.action(id) : -1;
}
int nativeTouchControlOf(int id) {
    return nativeTouchObserverInstalled() ? refs.ids.control(id) : -1;
}
int nativeTouchQuickOf(int id) {
    return nativeTouchObserverInstalled() ? refs.ids.quick(id) : -1;
}
int nativeTouchIndexByAction(int value) {
    return nativeTouchObserverInstalled() ? refs.ids.byAction(value) : -1;
}
int nativeTouchIndexByQuick(int value) {
    return nativeTouchObserverInstalled() ? refs.ids.byQuick(value) : -1;
}
int nativeTouchActionUnbound() {
    return nativeTouchObserverInstalled() ? refs.actionUnbound : -1;
}
int nativeTouchQuickUnbound() {
    return nativeTouchObserverInstalled() ? refs.quickUnbound : -1;
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
    Il2CppClass* layout = il2cpp::findClassQuiet(
        "", "VirtualControlsHardwareConfigurationMapping_Layout");
    Il2CppClass* quick = il2cpp::findClassQuiet("", "QuickActionButton_Layout");
    int unbound = -1, controllerCount = -1, quickUnbound = -1;
    if (!update || !menuField(&refs.gameMenu) ||
        !enumValue(controller, "ControlType", "Count", &controllerCount) ||
        !enumValue(layout, "ContolType", "Count", &unbound) ||
        !enumValue(quick, "QuickActionControl", "Unbound", &quickUnbound))
        return;
    refs.ids = resolveActionIds(unbound, controllerCount, quickUnbound, kMaxButtons);
    if (!refs.ids.valid()) return;
    refs.actionUnbound = unbound;
    refs.quickUnbound = quickUnbound;
    constexpr std::array<const char*, 3> fields{"Pressed", "WasPressed", "Scale"};
    for (size_t i = 0; i < fields.size(); ++i) {
        FieldInfo* field = controller ? il2cpp::findField(controller, fields[i]) : nullptr;
        if (!field) return;
        const size_t offset = api.field_get_offset(field);
        if (offset < sizeof(Il2CppObject) ||
            offset + sizeof(void*) > api.class_instance_size(controller)) return;
        refs.offsets[i] = offset;
        const Il2CppType* type = api.field_get_type(field);
        Il2CppClass* arrayType = type ? api.class_from_il2cpp_type(type) : nullptr;
        if (!arrayType || !api.class_get_rank ||
            api.class_get_rank(arrayType) != 1 ||
            !api.class_get_element_class || !api.class_get_name)
            return;
        Il2CppClass* element = api.class_get_element_class(arrayType);
        const char* expected = i == 2 ? "Single" : "Boolean";
        if (!element || std::strcmp(api.class_get_name(element), expected) != 0)
            return;
        refs.arrayTypes[i] = arrayType;
    }
    refs.controller = controller;
    if (!bl::hook::install(update, onControllerUpdate, &originalUpdate) ||
        !originalUpdate) return;
    installed.store(true, std::memory_order_release);
    // Importante: NÃO chamar setHostReady(true) aqui. O editor e o
    // salvamento ainda não foram conectados.
}

} // namespace bl::csharp::touch
