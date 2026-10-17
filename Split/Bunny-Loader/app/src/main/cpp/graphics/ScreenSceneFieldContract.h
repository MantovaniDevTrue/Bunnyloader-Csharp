#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/ValueLayout.h"
#include "core/Log.h"
#include <cstring>
#include <string>

namespace bl::graphics {
inline bool screenSceneValueContract(const Il2CppType* type, const char* expected,
                                     size_t bytes, const char* member) {
    auto& api = il2cpp::api();
    if (!type || !api.type_get_name || !api.class_from_il2cpp_type ||
        !api.class_is_valuetype || !api.class_value_size || !api.il2cpp_free) return false;
    char* actual = api.type_get_name(type);
    auto* klass = api.class_from_il2cpp_type(type);
    uint32_t alignment = 0;
    const int size = klass && api.class_is_valuetype(klass)
        ? api.class_value_size(klass, &alignment) : -1;
    const bool valid = actual && std::strcmp(actual, expected) == 0 &&
        size > 0 && static_cast<size_t>(size) == bytes;
    if (!valid)
        BL_WARN("[CSharp][WARN][NativeBridge] API=ScreenScene Member=%s Stage=BindRead Expected=%s:%zu Actual=%s:%d Result=SafeFallback Reason=NativeTypeOrSizeMismatch",
            member, expected, bytes, actual ? actual : "Missing", size);
    if (actual) api.il2cpp_free(actual);
    return valid;
}

inline FieldInfo* bindScreenSceneField(FieldInfo* field, const char* expected,
                                       size_t bytes, const char* member) {
    if (!field) return nullptr; // Optional fields can use the existing getter/fallback.
    auto& api = il2cpp::api();
    if (!api.field_get_flags || !api.field_get_type || !api.field_get_offset ||
        !api.field_static_get_value) return nullptr;
    const uint32_t flags = api.field_get_flags(field);
    const size_t offset = api.field_get_offset(field);
    if (!(flags & 0x10u) || offset >= 0x7fffffffu) {
        BL_WARN("[CSharp][WARN][NativeBridge] API=ScreenScene Member=%s Stage=BindRead Expected=StaticNonNegativeOffset ActualFlags=%u ActualOffset=%zu Result=SafeFallback Reason=NativeStorageMismatch",
            member, flags, offset);
        return nullptr;
    }
    return screenSceneValueContract(api.field_get_type(field), expected, bytes, member)
        ? field : nullptr;
}

inline bool screenSceneVectorContract(const Il2CppType* type, const char* member) {
    auto& api = il2cpp::api();
    if (!type || !api.type_get_name || !api.il2cpp_free ||
        !api.class_from_il2cpp_type || !api.class_get_field_from_name) return false;
    char* name = api.type_get_name(type);
    const std::string expected = name ? name : "Missing";
    if (name) api.il2cpp_free(name);
    if (expected != "Microsoft.Xna.Framework.Vector2" && expected != "UnityEngine.Vector2") return false;
    if (!screenSceneValueContract(type, expected.c_str(), 8, member)) return false;
    auto* klass = api.class_from_il2cpp_type(type);
    il2cpp::ValueLayout layout;
    std::string error;
    if (!il2cpp::resolveValueLayout(klass, &layout, &error)) {
        BL_WARN("[CSharp][WARN][NativeBridge] API=ScreenScene Member=%s Stage=BindRead Result=SafeFallback Detail=%s", member, error.c_str());
        return false;
    }
    for (int index = 0; index < 2; ++index) {
        auto* field = api.class_get_field_from_name(klass, index ? "Y" : "X");
        if (!field) field = api.class_get_field_from_name(klass, index ? "y" : "x");
        size_t offset = 0;
        if (!field || !screenSceneValueContract(api.field_get_type(field), "System.Single", 4, member) ||
            !layout.offset(api.field_get_offset(field), 4, &offset) || offset != size_t(index * 4)) return false;
    }
    return true;
}
inline FieldInfo* bindScreenSceneVectorField(FieldInfo* field, const char* member) {
    auto& api = il2cpp::api();
    if (!field || !screenSceneVectorContract(api.field_get_type(field), member)) return nullptr;
    char* name = api.type_get_name(api.field_get_type(field));
    auto* bound = name ? bindScreenSceneField(field, name, 8, member) : nullptr;
    if (name) api.il2cpp_free(name);
    return bound;
}
}
