#include "experiments/csharp/NativeTouchIconHooks.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchIcons.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::touch {
namespace {
using EditorFrameFn = void (*)(void*, int32_t, Il2CppObject**, void*, const MethodInfo*);
using QuickFrameFn = void (*)(int32_t, Il2CppObject**, void*, const MethodInfo*);
EditorFrameFn originalEditorFrame = nullptr;
QuickFrameFn originalQuickFrame = nullptr;
using DrawFn = void (*)(int32_t, Il2CppObject*, uint8_t, float*, uint8_t,
                        uint8_t, const MethodInfo*);
using DrawIconFn = void (*)(Il2CppObject*, Il2CppObject*, int32_t,
                            float*, const MethodInfo*);
DrawFn originalDraw = nullptr;
DrawIconFn drawIcon = nullptr;
const MethodInfo* drawIconMethod = nullptr;
std::atomic<bool> installed{false};
std::atomic<bool> drawInstalled{false};
bool attempted = false;
bool drawAttempted = false;

bool enumParam(const MethodInfo* method, const char* outerName,
               const char* innerName) {
    auto& api = il2cpp::api();
    Il2CppClass* parent = il2cpp::findClassQuiet("", outerName);
    Il2CppClass* type = api.class_from_il2cpp_type(api.method_get_param(method, 0));
    if (!type || !parent || !api.class_get_declaring_type ||
        !api.class_is_enum || !api.class_get_name ||
        !api.class_enum_basetype || !api.type_get_name ||
        !api.il2cpp_free || !api.class_is_enum(type) ||
        api.class_get_declaring_type(type) != parent ||
        std::strcmp(api.class_get_name(type), innerName) != 0)
        return false;
    char* raw = api.type_get_name(api.class_enum_basetype(type));
    const bool correct = raw && std::strcmp(raw, "System.Int32") == 0;
    if (raw) api.il2cpp_free(raw);
    return correct;
}

bool validateMethod(const MethodInfo* method, bool instance,
                    const char* outerName, const char* innerName) {
    auto& api = il2cpp::api();
    if (!method || !api.method_is_instance || !api.method_get_param_count ||
        !api.method_get_param || !api.method_get_return_type ||
        !api.class_from_il2cpp_type ||
        !api.method_is_instance(method) != !instance ||
        api.method_get_param_count(method) != 3 ||
        !il2cpp::methodPointer(method) ||
        !enumParam(method, outerName, innerName)) return false;
    char* name = api.type_get_name(api.method_get_return_type(method));
    const bool returnsVoid = name && std::strcmp(name, "System.Void") == 0;
    if (name) api.il2cpp_free(name);
    if (!returnsVoid) return false;
    char* tex = api.type_get_name(api.method_get_param(method, 1));
    char* rect = api.type_get_name(api.method_get_param(method, 2));
    const bool shape = tex && rect && std::strstr(tex, "Texture2D") &&
                       std::strchr(tex, '&') &&
                       std::strstr(rect, "Rectangle") && std::strchr(rect, '&');
    if (tex) api.il2cpp_free(tex);
    if (rect) api.il2cpp_free(rect);
    return shape;
}

bool rectangleLayout() {
    auto& api = il2cpp::api();
    Il2CppClass* rect = il2cpp::findClass({"Microsoft.Xna.Framework", "Rectangle", {}});
    if (!rect || !api.class_is_valuetype || !api.class_is_valuetype(rect) ||
        !api.field_get_offset || !api.field_get_type ||
        !api.type_get_name || !api.il2cpp_free) return false;
    il2cpp::ValueLayout shape{};
    std::string error;
    if (!il2cpp::resolveValueLayout(rect, &shape, &error) || shape.size != 16)
        return false;
    constexpr const char* members[4] = {"X", "Y", "Width", "Height"};
    for (int i = 0; i < 4; ++i) {
        FieldInfo* field = il2cpp::findField(rect, members[i]);
        if (!field) return false;
        char* fieldType = api.type_get_name(api.field_get_type(field));
        bool isInt = fieldType && std::strcmp(fieldType, "System.Int32") == 0;
        if (fieldType) api.il2cpp_free(fieldType);
        if (!isInt || api.field_get_offset(field) != shape.header +
            static_cast<size_t>(i * 4)) return false;
    }
    return true;
}

bool outputIcon(int id, Il2CppObject** texture, void* frame) {
    if (!texture || !frame) return false;
    int w = 0, h = 0;
    Il2CppObject* asset = nativeTouchIcon(id, &w, &h);
    if (!asset || w < 1 || h < 1) return false;
    *texture = asset;
    const int32_t area[4] = {0, 0, w, h};
    std::memcpy(frame, area, sizeof(area));
    return true;
}

void editorFrame(void* self, int32_t action, Il2CppObject** texture,
                 void* frame, const MethodInfo* method) {
    const int id = nativeTouchIndexByAction(action);
    EditorFrameFn original = __atomic_load_n(&originalEditorFrame, __ATOMIC_ACQUIRE);
    if (id >= 0) {
        if (outputIcon(id, texture, frame)) return;
        int unbound = nativeTouchActionUnbound();
        if (original && unbound >= 0)
            return original(self, unbound, texture, frame, method);
        return;
    }
    if (original) original(self, action, texture, frame, method);
}

void quickFrame(int32_t quick, Il2CppObject** texture,
                void* frame, const MethodInfo* method) {
    const int id = nativeTouchIndexByQuick(quick);
    QuickFrameFn original = __atomic_load_n(&originalQuickFrame, __ATOMIC_ACQUIRE);
    if (id >= 0) {
        if (outputIcon(id, texture, frame)) return;
        int unbound = nativeTouchQuickUnbound();
        if (original && unbound >= 0)
            return original(unbound, texture, frame, method);
        return;
    }
    if (original) original(quick, texture, frame, method);
}
void quickDraw(int32_t action, Il2CppObject* layout, uint8_t pressed,
               float* scale, uint8_t ignoreChecks, uint8_t ignoreIcon,
               const MethodInfo* method) {
    DrawFn original = __atomic_load_n(&originalDraw, __ATOMIC_ACQUIRE);
    const int id = nativeTouchIndexByQuick(action);
    if (id < 0) {
        if (original)
            original(action, layout, pressed, scale, ignoreChecks, ignoreIcon, method);
        return;
    }
    int width = 0, height = 0;
    Il2CppObject* texture = !ignoreIcon && layout && scale
        ? nativeTouchIcon(id, &width, &height) : nullptr;
    if (texture && drawIcon && drawIconMethod) {
        const int32_t state = !registry().enabled(id) ? 0 : pressed ? 1 : 2;
        drawIcon(layout, texture, state, scale, drawIconMethod);
        return;
    }
    const int fallback = nativeTouchQuickUnbound();
    if (original && fallback >= 0)
        original(fallback, layout, pressed, scale, ignoreChecks, ignoreIcon, method);
}
}

bool nativeTouchIconFrameHooksInstalled() {
    return installed.load(std::memory_order_acquire);
}
bool nativeTouchQuickDrawHookInstalled() {
    return drawInstalled.load(std::memory_order_acquire);
}

void pumpNativeTouchQuickDrawHookOnGameThread() {
    if (drawAttempted || nativeTouchQuickDrawHookInstalled() ||
        !nativeTouchIconFrameHooksInstalled()) return;
    drawAttempted = true;
    auto& api = il2cpp::api();
    Il2CppClass* quick = il2cpp::findClassQuiet("", "GUIQuickActionButton");
    const MethodInfo* full = quick ? il2cpp::findMethodBySignature(
        quick, il2cpp::parseSignature(
            "void Draw(QuickActionButton_Layout.QuickActionControl actionType, QuickActionButton_Layout layout, bool pressed, ref float scale, bool ignoreChecks, bool ignoreIcon)")) : nullptr;
    const MethodInfo* icon = quick ? il2cpp::findMethodBySignature(
        quick, il2cpp::parseSignature(
            "void Draw(QuickActionButton_Layout layout, Texture2D buttonTexture, GUIQuickActionButton.InteractionState state, ref float scale)")) : nullptr;
    if (!full || !icon || !api.method_is_instance ||
        api.method_is_instance(full) || api.method_is_instance(icon) ||
        !api.method_get_param_count ||
        api.method_get_param_count(full) != 6 ||
        api.method_get_param_count(icon) != 4 ||
        !il2cpp::methodPointer(full) || !il2cpp::methodPointer(icon))
        return;
    // As duas assinaturas são resolvidas integralmente pelo Resolver.
    // Não encaminhar outro tipo de Draw nem presumir enum no jogo.
    drawIcon = reinterpret_cast<DrawIconFn>(il2cpp::methodPointer(icon));
    drawIconMethod = icon;
    if (!bl::hook::install(full, quickDraw, &originalDraw) ||
        !originalDraw) return;
    drawInstalled.store(true, std::memory_order_release);
}
void pumpNativeTouchIconFrameHooksOnGameThread() {
    if (attempted || nativeTouchIconFrameHooksInstalled() ||
        !nativeTouchObserverInstalled() || !rectangleLayout())
        return;
    attempted = true;
    auto& api = il2cpp::api();
    if (!api.method_get_return_type || !api.type_get_name ||
        !api.il2cpp_free || !api.class_get_declaring_type) return;
    Il2CppClass* editor = il2cpp::findClassQuiet("", "GUIVirtualInputControllerEdit");
    Il2CppClass* quick = il2cpp::findClassQuiet("", "GUIQuickActionButton");
    const MethodInfo* edit = editor ? il2cpp::findMethodBySignature(
        editor, il2cpp::parseSignature(
            "void GetControlTextureFrame(VirtualControlsHardwareConfigurationMapping_Layout.ContolType controlType, out Texture2D tex, out Rectangle frame)")) : nullptr;
    const MethodInfo* icon = quick ? il2cpp::findMethodBySignature(
        quick, il2cpp::parseSignature(
            "void GetIconFrame(QuickActionButton_Layout.QuickActionControl actionType, out Texture2D iconTexture, out Rectangle frame)")) : nullptr;
    if (!validateMethod(edit, true,
                        "VirtualControlsHardwareConfigurationMapping_Layout", "ContolType") ||
        !validateMethod(icon, false, "QuickActionButton_Layout", "QuickActionControl"))
        return;
    if (!bl::hook::install(edit, editorFrame, &originalEditorFrame) ||
        !originalEditorFrame) return;
    if (!bl::hook::install(icon, quickFrame, &originalQuickFrame) ||
        !originalQuickFrame) return;
    installed.store(true, std::memory_order_release);
    // Ainda faltam Draw/HandleTouchCursorDown e persistência IL2CPP real.
}
} // namespace bl::csharp::touch
