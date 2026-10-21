#include "experiments/csharp/NativeTouchEditor.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::touch {
namespace {
using NameFn = Il2CppString* (*)(void*, int32_t, const MethodInfo*);
NameFn originalName = nullptr;
std::atomic<bool> installed{false};
bool attempted = false;

struct Label {
    std::string value;
    uint32_t root = 0;
};
std::array<Label, kMaxButtons> labels;

bool validSignature(const MethodInfo* method) {
    auto& api = il2cpp::api();
    if (!method || !api.method_is_instance || !api.method_get_param_count ||
        !api.method_get_param || !api.method_get_return_type ||
        !api.type_get_name || !api.il2cpp_free ||
        !api.method_is_instance(method) ||
        api.method_get_param_count(method) != 1 ||
        !il2cpp::methodPointer(method)) return false;

    char* result = api.type_get_name(api.method_get_return_type(method));
    bool stringReturn = result && std::strcmp(result, "System.String") == 0;
    if (result) api.il2cpp_free(result);
    if (!stringReturn) return false;
    if (!api.class_from_il2cpp_type || !api.class_get_name ||
        !api.class_get_declaring_type || !api.class_is_enum ||
        !api.class_enum_basetype) return false;
    Il2CppClass* parameter = api.class_from_il2cpp_type(
        api.method_get_param(method, 0));
    Il2CppClass* mapping = il2cpp::findClassQuiet(
        "", "VirtualControlsHardwareConfigurationMapping_Layout");
    if (!parameter || !mapping || !api.class_is_enum(parameter) ||
        api.class_get_declaring_type(parameter) != mapping ||
        std::strcmp(api.class_get_name(parameter), "ContolType") != 0)
        return false;
    char* base = api.type_get_name(api.class_enum_basetype(parameter));
    bool enumArg = base && std::strcmp(base, "System.Int32") == 0;
    if (base) api.il2cpp_free(base);
    return enumArg;
}

Il2CppString* labelString(int id) {
    auto& api = il2cpp::api();
    if (!api.string_new || !api.gchandle_new ||
        !api.gchandle_get_target || !api.gchandle_free)
        return nullptr;
    std::string text = registry().displayName(id);
    if (text.empty()) text = "Indisponível";
    Label& cached = labels[static_cast<size_t>(id)];
    if (cached.root && cached.value == text)
        if (auto* value = api.gchandle_get_target(cached.root))
            return reinterpret_cast<Il2CppString*>(value);

    if (cached.root) {
        api.gchandle_free(cached.root);
        cached.root = 0;
    }
    cached.value.clear();
    Il2CppString* result = api.string_new(text.c_str());
    if (!result) return nullptr;
    const uint32_t root = api.gchandle_new(
        reinterpret_cast<Il2CppObject*>(result), false);
    if (root) {
        cached.value = text;
        cached.root = root;
    }
    return result;
}

Il2CppString* onGetControlName(void* self, int32_t control,
                              const MethodInfo* method) {
    const int id = nativeTouchIndexByAction(control);
    if (id >= 0 && id < kMaxButtons) {
        if (Il2CppString* value = labelString(id)) return value;
        // Se não der para construir o rótulo, devolve o resultado vanilla.
    }
    NameFn original = __atomic_load_n(&originalName, __ATOMIC_ACQUIRE);
    return original ? original(self, control, method) : nullptr;
}
} // namespace

bool nativeTouchEditorLabelsInstalled() {
    return installed.load(std::memory_order_acquire);
}

void pumpNativeTouchEditorLabelsOnGameThread() {
    if (attempted || nativeTouchEditorLabelsInstalled() ||
        !nativeTouchObserverInstalled() || registry().count() == 0) return;
    attempted = true;
    auto& api = il2cpp::api();
    if (!api.string_new || !api.gchandle_new || !api.gchandle_free ||
        !api.gchandle_get_target) return;
    Il2CppClass* editor = il2cpp::findClassQuiet("", "GUIVirtualInputControllerEdit");
    const MethodInfo* method = editor ? il2cpp::findMethodBySignature(
        editor, il2cpp::parseSignature(
            "string GetControlName(VirtualControlsHardwareConfigurationMapping_Layout.ContolType control)"))
        : nullptr;
    if (!validSignature(method)) return;
    if (!bl::hook::install(method, onGetControlName, &originalName) ||
        !originalName) return;
    installed.store(true, std::memory_order_release);
    // Exibir os nomes não equivale a salvar perfis ou habilitar o botão.
    // host_ready continua falso até todos os módulos estarem conectados.
}
} // namespace bl::csharp::touch
