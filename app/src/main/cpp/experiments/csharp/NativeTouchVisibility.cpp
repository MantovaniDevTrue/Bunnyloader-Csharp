#include "experiments/csharp/NativeTouchVisibility.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <atomic>
#include <cstdint>
#include <cstring>

namespace bl::csharp::touch {
namespace {
using VisibleFn = bool (*)(void*, int32_t, int32_t, Il2CppObject*,
                           const MethodInfo*);
VisibleFn original = nullptr;
std::atomic<bool> installed{false};
bool attempted = false;

bool check(const MethodInfo* method) {
    auto& a = il2cpp::api();
    if (!method || !a.method_is_instance ||
        !a.method_is_instance(method) ||
        !a.method_get_param_count ||
        a.method_get_param_count(method) != 3 ||
        !a.class_from_il2cpp_type || !a.method_get_param ||
        !a.class_is_enum || !a.class_get_declaring_type ||
        !a.class_get_name || !a.class_is_valuetype ||
        !a.class_enum_basetype ||
        !a.method_get_return_type || !a.type_get_name ||
        !a.il2cpp_free || !il2cpp::methodPointer(method)) return false;
    Il2CppClass* number = a.class_from_il2cpp_type(a.method_get_param(method, 0));
    Il2CppClass* action = a.class_from_il2cpp_type(a.method_get_param(method, 1));
    Il2CppClass* layout = a.class_from_il2cpp_type(a.method_get_param(method, 2));
    Il2CppClass* mapping = il2cpp::findClassQuiet(
        "", "VirtualControlsHardwareConfigurationMapping_Layout");
    if (!number || !action || !layout || !mapping ||
        std::strcmp(a.class_get_name(number), "Int32") != 0 ||
        !a.class_is_enum(action) ||
        a.class_get_declaring_type(action) != mapping ||
        std::strcmp(a.class_get_name(action), "ContolType") != 0 ||
        layout != mapping || a.class_is_valuetype(layout)) return false;
    char* base = a.type_get_name(a.class_enum_basetype(action));
    const bool int32Action = base && std::strcmp(base, "System.Int32") == 0;
    if (base) a.il2cpp_free(base);
    if (!int32Action) return false;
    char* ret = a.type_get_name(a.method_get_return_type(method));
    const bool boolean = ret && std::strcmp(ret, "System.Boolean") == 0;
    if (ret) a.il2cpp_free(ret);
    return boolean;
}

bool onVisible(void* self, int32_t slot, int32_t raw, Il2CppObject* mapping,
               const MethodInfo* method) {
    const int id = nativeTouchIndexByAction(raw);
    if (id >= 0 && !registry().visible(id)) return false;
    VisibleFn previous = __atomic_load_n(&original, __ATOMIC_ACQUIRE);
    return previous ? previous(self, slot, raw, mapping, method) : false;
}
}

bool nativeTouchVisibilityHookInstalled() {
    return installed.load(std::memory_order_acquire);
}
void pumpNativeTouchVisibilityHookOnGameThread() {
    if (attempted || nativeTouchVisibilityHookInstalled() ||
        !nativeTouchObserverInstalled()) return;
    static uint32_t probes = 0;
    if (++probes != 1 && probes % 120 != 0) return;
    Il2CppClass* controller = il2cpp::findClassQuiet("", "GUIVirtualInputController");
    const MethodInfo* method = controller ? il2cpp::findMethodBySignature(
        controller, il2cpp::parseSignature(
            "bool DetermineButtonVisible(int slot, VirtualControlsHardwareConfigurationMapping_Layout.ContolType action, VirtualControlsHardwareConfigurationMapping_Layout slotConfig)"))
        : nullptr;
    if (!check(method)) return;
    attempted = true;
    if (!bl::hook::install(method, onVisible, &original) ||
        !original) return;
    installed.store(true, std::memory_order_release);
}

} // namespace bl::csharp::touch
