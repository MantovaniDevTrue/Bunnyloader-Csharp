#pragma once

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <cstdint>
#include <cstring>

namespace bl::il2cpp {

inline bool readInt32Constant(FieldInfo* field, Il2CppClass* expected, int32_t* value) {
    if (!field || !expected || !value) return false;
    auto& a=api();
    if (!a.field_get_flags || !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.object_get_class || !a.field_static_get_value ||
        a.class_from_il2cpp_type(a.field_get_type(field))!=expected ||
        (a.field_get_flags(field)&0x10u)==0)
        return false;

    int32_t result=0;
    if ((a.field_get_flags(field)&0x40u)!=0) {
        if (!a.field_get_value_object) return false;
        Il2CppObject* boxed=a.field_get_value_object(field,nullptr);
        if (!boxed || a.object_get_class(boxed)!=expected) return false;
        std::memcpy(&result,reinterpret_cast<const char*>(boxed)+sizeof(Il2CppObject),
                    sizeof(result));
    } else {
        a.field_static_get_value(field,&result);
    }
    *value=result;
    return true;
}

} // namespace bl::il2cpp
