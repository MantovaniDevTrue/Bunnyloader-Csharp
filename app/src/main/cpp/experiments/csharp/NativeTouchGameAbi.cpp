#include "experiments/csharp/NativeTouchGameAbi.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cstring>

namespace bl::csharp::touch {
namespace {

bool fieldArray(Il2CppClass* controller, const char* name, const char* expected) {
    auto& a = il2cpp::api();
    if (!controller || !a.field_get_flags || !a.field_get_offset ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_rank || !a.class_get_element_class || !a.class_get_name ||
        !a.class_instance_size) return false;

    FieldInfo* field = il2cpp::findField(controller, name);
    if (!field || (a.field_get_flags(field) & 0x10u)) return false;
    size_t offset = a.field_get_offset(field);
    if (offset < sizeof(Il2CppObject) ||
        offset + sizeof(void*) > a.class_instance_size(controller))
        return false;
    Il2CppClass* arrayType = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!arrayType || a.class_get_rank(arrayType) != 1) return false;
    Il2CppClass* element = a.class_get_element_class(arrayType);
    return element && a.class_get_name(element) &&
           std::strcmp(a.class_get_name(element), expected) == 0;
}

bool signature(Il2CppClass* type, const char* definition, bool instance) {
    if (!type) return false;
    const MethodInfo* method = il2cpp::findMethodBySignature(
        type, il2cpp::parseSignature(definition));
    if (!method) return false;
    const auto& a = il2cpp::api();
    return a.method_is_instance && a.method_is_instance(method) == instance &&
           il2cpp::methodPointer(method) != nullptr;
}

bool methodName(Il2CppClass* type, const char* name, unsigned arguments, bool instance) {
    if (!type) return false;
    const auto& a = il2cpp::api();
    if (!a.class_get_method_from_name || !a.method_is_instance ||
        !a.method_get_param_count) return false;
    const MethodInfo* method =
        a.class_get_method_from_name(type, name, static_cast<int>(arguments));
    return method && a.method_get_param_count(method) == arguments &&
           a.method_is_instance(method) == instance &&
           il2cpp::methodPointer(method) != nullptr;
}

} // namespace

HostAbiCheck inspectNativeTouchAbi() {
    HostAbiCheck result;
    auto& a = il2cpp::api();
    if (!a.gameImage || !a.class_get_method_from_name ||
        !a.method_is_instance || !a.method_get_param_count) {
        result.diagnostic = "Il2CppApiUnavailable";
        return result;
    }
    Il2CppClass* controller = il2cpp::findClassQuiet("", "GUIVirtualInputController");
    Il2CppClass* editor = il2cpp::findClassQuiet("", "GUIVirtualInputControllerEdit");
    Il2CppClass* styles = il2cpp::findClassQuiet("", "VirtualControlsStyles_Layout");
    Il2CppClass* quick = il2cpp::findClassQuiet("", "GUIQuickActionButton");
    Il2CppClass* mapping = il2cpp::findClassQuiet(
        "", "VirtualControlsHardwareConfigurationMapping_Layout");

    result.controller = signature(controller, "void Update(float elapsedTime)", true);
    result.arrays = result.controller &&
        fieldArray(controller, "Pressed", "Boolean") &&
        fieldArray(controller, "WasPressed", "Boolean") &&
        fieldArray(controller, "Scale", "Single");

    // O método tem structs IL2CPP nos argumentos. Nunca instale um hook
    // de input se o layout/ABI dele não foi confirmado no Android.
    result.touch = methodName(controller, "HandleTouchCursorDown", 3, true);
    result.editor = methodName(editor, "GetControlName", 1, true) &&
                    methodName(editor, "GetControlTextureFrame", 3, true) &&
                    methodName(editor, "Draw", 0, true) &&
                    methodName(quick, "GetIconFrame", 3, false);
    result.layouts = methodName(styles, "SaveConfiguration", 1, true) &&
                     methodName(styles, "LoadConfiguration", 1, true) &&
                     methodName(styles, "GetShareCode", 1, false);
    result.group = mapping && il2cpp::findNested(mapping, "ContolType") &&
                   il2cpp::findNested(controller, "ControlType") &&
                   methodName(controller, "Draw", 0, true);

    if (!result.controller) result.diagnostic = "ControllerUpdateABI";
    else if (!result.arrays) result.diagnostic = "ControllerArrayLayout";
    else if (!result.touch) result.diagnostic = "TouchCursorDownABI";
    else if (!result.editor) result.diagnostic = "EditorActionsABI";
    else if (!result.layouts) result.diagnostic = "LayoutPersistenceABI";
    else if (!result.group) result.diagnostic = "GroupAndEnumABI";
    else result.diagnostic = "AllMethodsLocatedHookInstallationPending";
    return result;
}

} // namespace bl::csharp::touch
