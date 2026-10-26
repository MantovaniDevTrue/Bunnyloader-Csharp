#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/NativePointerContract.h"
#include <cstring>
#include <string>
#include <limits>

namespace bl::il2cpp {
// Resolve once at binding time; never accept an offset without storage/type/bounds.
inline bool nativeFieldContract(Il2CppClass* owner,const char* name,const char* expected,
                                bool isStatic,size_t width,FieldInfo** result,int* offset,
                                std::string* error) {
    auto& a=il2cpp::api();
    if(!a.class_from_il2cpp_type||!a.field_get_type||!a.field_get_flags||
       !a.class_get_name||!a.class_get_namespace||!a.class_is_valuetype||
       (!isStatic&&(!a.class_instance_size||!a.field_get_offset))) {
        *error="Field="+std::string(name)+" Reason=RequiredExportMissing";return false;
    }
    auto* field=owner?il2cpp::findField(owner,name):nullptr;
    auto* type=field?a.class_from_il2cpp_type(a.field_get_type(field)):nullptr;
    const char* ns=type?a.class_get_namespace(type):nullptr;
    const char* cls=type?a.class_get_name(type):nullptr;
    const std::string actual=type?std::string(ns?ns:"")+"."+(cls?cls:"<unnamed>"):"Missing";
    if(!field||actual!=expected||bool(a.field_get_flags(field)&0x10u)!=isStatic) {
        *error="Field="+std::string(name)+" Expected="+(isStatic?"Static:":"Instance:")+expected+" Actual="+actual+" Reason=NativeTypeOrStorageMismatch";return false;
    }
    if(width!=sizeof(void*)||a.class_is_valuetype(type)) {
        uint32_t align=0;
        if(!a.class_value_size||!a.class_is_valuetype(type)||a.class_value_size(type,&align)!=static_cast<int>(width)) {
            *error="Field="+std::string(name)+" ExpectedBytes="+std::to_string(width)+" Reason=NativeSizeMismatch";return false;
        }
    }
    int location=-1;
    if(!isStatic) {
        const size_t value=a.field_get_offset(field),size=a.class_instance_size(owner);
        if(value<sizeof(Il2CppObject)||size<width||value>size-width||value>static_cast<size_t>(INT32_MAX)) {
            *error="Field="+std::string(name)+" Offset="+std::to_string(value)+" InstanceSize="+std::to_string(size)+" Reason=InvalidFieldBounds";return false;
        }
        location=static_cast<int>(value);
    }
    if(result)*result=field;
    if(offset)*offset=location;
    return true;
}
inline bool readNativeId(Il2CppClass* owner,const char* name,int* result,std::string* error) {
    auto& a=api();auto* f=owner?findField(owner,name):nullptr;
    auto* type=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;
    if(!f||!(a.field_get_flags(f)&0x10u)||!type){*error="Field="+std::string(name)+" Reason=MissingStaticId";return false;}
    if(a.class_is_enum&&a.class_is_enum(type)&&a.class_enum_basetype)type=a.class_from_il2cpp_type(a.class_enum_basetype(type));
    const char* ns=type?a.class_get_namespace(type):nullptr;const char* actual=type?a.class_get_name(type):nullptr;
    struct Kind{const char* name;int bytes;bool sign;};
    const Kind kinds[]={{"Byte",1,false},{"SByte",1,true},{"UInt16",2,false},{"Int16",2,true},{"UInt32",4,false},{"Int32",4,true}};
    for(const auto& kind:kinds)if(ns&&actual&&!std::strcmp(ns,"System")&&!std::strcmp(actual,kind.name)) {
        uint32_t align=0;
        if(!a.class_is_valuetype(type)||a.class_value_size(type,&align)!=kind.bytes)break;
        uint64_t raw=0;a.field_static_get_value(f,&raw);int64_t value=static_cast<int64_t>(raw);
        if(kind.sign){if(kind.bytes==1)value=static_cast<int8_t>(raw);else if(kind.bytes==2)value=static_cast<int16_t>(raw);else value=static_cast<int32_t>(raw);}
        if(value<0||value>INT32_MAX){*error="Field="+std::string(name)+" Actual="+std::to_string(value)+" Reason=NativeIdOutOfRange";return false;}
        *result=static_cast<int>(value);return true;
    }
    *error="Field="+std::string(name)+" Expected=IntegerId Actual="+(actual?actual:"Missing")+" Reason=NativeIdTypeMismatch";return false;
}

inline bool nativeArrayContract(Il2CppArray* array,const char* element,size_t width,uintptr_t maximum=100000) {
    if(!nativeManagedPointerPlausible(array))return false;
    auto& a=il2cpp::api();
    if(!nativeManagedPointerPlausible(array->obj.klass)||
       array->length>maximum||!a.class_get_rank||!a.class_get_element_class||
       !a.class_value_size||!a.class_is_valuetype||!a.class_get_name||!a.class_get_namespace||
       a.class_get_rank(array->obj.klass)!=1)return false;
    auto* cls=a.class_get_element_class(array->obj.klass);uint32_t align=0;
    const char* ns=cls?a.class_get_namespace(cls):nullptr;const char* name=cls?a.class_get_name(cls):nullptr;
    return cls&&ns&&name&&std::strcmp(ns,"System")==0&&std::strcmp(name,element)==0&&
        a.class_is_valuetype(cls)&&a.class_value_size(cls,&align)==static_cast<int>(width);
}
}
