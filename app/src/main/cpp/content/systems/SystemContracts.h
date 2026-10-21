#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cstring>
#include <string>

namespace bl::runtime::systems {
enum class SystemStageFieldKind { Boolean, Single, Double, Int32 };
// Static field APIs copy the complete native value. A matching spelling alone
// is insufficient: reject instance/literal fields and validate the value size.
inline FieldInfo* systemStageField(Il2CppClass* owner, const char* name,
                                  bool numeric, SystemStageFieldKind* kind, std::string* error,
                                  const char* apiName = "BunnyWorldUpdateLoader") {
    auto& a = il2cpp::api();
    const std::string expected = numeric ? "StaticMutable:System.Single:4|System.Double:8|System.Int32:4" : "Static:System.Boolean:1";
    const auto fail = [&](const std::string& actual, const char* reason) -> FieldInfo* {
        *error = "[Native][ERROR][NativeBridge] API=" + std::string(apiName) + " Field=" + std::string(name) +
            " Stage=BindSystemField Expected=" + expected + " Actual=" + actual +
            " Result=Rejected Reason=" + reason;
        return nullptr;
    };
    if (!owner || !a.field_get_flags || !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_name || !a.class_get_namespace || !a.class_is_valuetype ||
        !a.class_value_size || !a.field_static_get_value || (numeric && !a.field_static_set_value))
        return fail("MissingMetadataExportsOrOwner", "MetadataUnavailable");
    auto* field = il2cpp::findField(owner, name);
    if (!field) return fail("Missing", "RequiredFieldMissing");
    const auto flags = a.field_get_flags(field);
    if (!(flags & 0x10u) || (numeric && (flags & (0x20u | 0x40u))))
        return fail("Flags:" + std::to_string(flags), "NativeStorageMismatch");
    const auto* type = a.field_get_type(field);
    auto* cls = type ? a.class_from_il2cpp_type(type) : nullptr;
    if (!cls) return fail("MissingType", "NativeTypeMismatch");
    const char* ns = a.class_get_namespace(cls);
    const char* found = a.class_get_name(cls);
    uint32_t alignment = 0;
    const int size = a.class_is_valuetype(cls) ? a.class_value_size(cls, &alignment) : -1;
    const bool doubleValue = found && !std::strcmp(found,"Double");
    const bool integerValue = found && !std::strcmp(found,"Int32");
    const bool valid = ns && !std::strcmp(ns,"System") && found && (numeric ?
        (doubleValue && size == 8) || ((!std::strcmp(found,"Single") || integerValue) && size == 4) :
        !std::strcmp(found,"Boolean") && size == 1);
    if (!valid) return fail(std::string(ns ? ns : "?") + "." + (found ? found : "?") +
        ":" + std::to_string(size), "NativeTypeOrSizeMismatch");
    if (kind) *kind = !numeric ? SystemStageFieldKind::Boolean : doubleValue ? SystemStageFieldKind::Double :
        integerValue ? SystemStageFieldKind::Int32 : SystemStageFieldKind::Single;
    return field;
}

inline bool systemStageMethodStorage(const MethodInfo* method, bool isStatic, const char* name,
                                     std::string* error) {
    auto& a = il2cpp::api();
    if (method && a.method_get_flags && bool(a.method_get_flags(method,nullptr) & 0x10u) == isStatic) return true;
    *error = "[CSharp][ERROR][NativeBridge] API=BunnyWorldUpdateLoader Method=" + std::string(name) +
        " Stage=BindSystemMethod Expected=" + (isStatic ? "Static" : "Instance") + " Actual=" +
        (!method ? "Missing" : !a.method_get_flags ? "MissingMetadataExports" :
         (a.method_get_flags(method,nullptr) & 0x10u) ? "Static" : "Instance") +
        " Result=Rejected Reason=NativeMethodStorageMismatch";
    return false;
}
}
