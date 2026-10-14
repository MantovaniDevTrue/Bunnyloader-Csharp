#pragma once
#include "il2cpp/Api.h"
#include <cstring>
#include <string>
#include <vector>

namespace bl::il2cpp {
struct ValueLayout {
    size_t size=0, header=0;
    bool offset(size_t raw,size_t width,size_t* value) const {
        if(raw<header||width>size||raw-header>size-width)return false;
        *value=raw-header;return true;
    }
};
// Metadata may report offsets in a boxed object or directly in its value.
// Accept a layout only if every field fits one unambiguous convention.
inline bool resolveValueLayout(Il2CppClass* cls,ValueLayout* layout,std::string* error) {
    auto& a=api();
    auto fail=[&](const char* reason,const std::string& actual) {
        const char* name=cls&&a.class_get_name?a.class_get_name(cls):nullptr;
        *error=std::string("[CSharp][ERROR][NativeBridge] API=ValueLayout Type=")+(name?name:"Missing")+
            " Stage=BindLayout Expected=UnambiguousBoundedValueLayout Actual="+actual+" Result=Rejected Reason="+reason;
        return false;
    };
    if(!cls||!a.class_is_valuetype||!a.class_is_valuetype(cls)||!a.class_value_size||!a.class_get_fields||
       !a.field_get_type||!a.field_get_flags||!a.field_get_offset||!a.class_from_il2cpp_type)
        return fail("MetadataUnavailable","RequiredExportsOrValueTypeMissing");
    uint32_t align=0;const auto bytes=a.class_value_size(cls,&align);
    if(bytes<=0||bytes>65536)return fail("InvalidValueSize",std::to_string(bytes));
    const auto size=static_cast<size_t>(bytes);bool rawFits=true,boxedFits=true;size_t count=0;
    void* it=nullptr;
    while(auto* field=a.class_get_fields(cls,&it)) {
        if(a.field_get_flags(field)&0x10u)continue;
        auto* type=a.class_from_il2cpp_type(a.field_get_type(field));
        if(!type)return fail("FieldTypeMissing","Missing");
        uint32_t fieldAlign=0;
        const auto n=a.class_is_valuetype(type)?a.class_value_size(type,&fieldAlign):static_cast<int>(sizeof(void*));
        if(n<=0)return fail("InvalidFieldSize",std::to_string(n));
        const auto width=static_cast<size_t>(n),off=a.field_get_offset(field);
        rawFits=rawFits&&width<=size&&off<=size-width;
        boxedFits=boxedFits&&off>=sizeof(Il2CppObject)&&width<=size&&off-sizeof(Il2CppObject)<=size-width;
        ++count;
    }
    if(!count||rawFits==boxedFits)
        return fail(rawFits?"AmbiguousFieldLayout":"FieldOutsideValue",std::to_string(size)+"Bytes,Fields="+std::to_string(count));
    *layout={size,boxedFits?sizeof(Il2CppObject):0};return true;
}
inline bool valuePrimitive(Il2CppClass* cls) {
    auto& a=api();if(!cls||!a.class_get_namespace||!a.class_get_name)return false;
    const char* ns=a.class_get_namespace(cls),*name=a.class_get_name(cls);
    if(!ns||!name||std::strcmp(ns,"System")!=0)return false;
    for(const char* n:{"Boolean","Char","Byte","SByte","Int16","UInt16","Int32","UInt32",
                        "Int64","UInt64","Single","Double","IntPtr","UIntPtr"})
        if(std::strcmp(name,n)==0)return true;
    return false;
}
inline bool valueReferenceOffsets(Il2CppClass* cls,size_t base,std::vector<size_t>* out,std::string* error,unsigned depth=0) {
    auto& a=api();
    if(!cls||!a.class_get_name||!a.class_get_namespace) {
        *error="[CSharp][ERROR][NativeBridge] API=ValueLayout Stage=BindReferences Expected=TypeAndNameMetadata Actual=Missing:"+
            std::string(!cls?"Type ":"")+(!a.class_get_name?"class_get_name ":"")+(!a.class_get_namespace?"class_get_namespace":"")+
            " Result=Rejected Reason=MetadataUnavailable";return false;
    }
    if(valuePrimitive(cls)||(a.class_is_enum&&a.class_is_enum(cls)))return true;
    if(depth>=8){*error="[CSharp][ERROR][NativeBridge] API=ValueLayout Stage=BindReferences Result=Rejected Reason=ValueNestingExceeded";return false;}
    ValueLayout layout;if(!resolveValueLayout(cls,&layout,error))return false;
    void* it=nullptr;
    while(auto* field=a.class_get_fields(cls,&it)) {
        if(a.field_get_flags(field)&0x10u)continue;
        auto* type=a.class_from_il2cpp_type(a.field_get_type(field));uint32_t align=0;
        if(!type){*error="[CSharp][ERROR][NativeBridge] API=ValueLayout Stage=BindReferences Expected=FieldType Actual=Missing Result=Rejected Reason=MetadataUnavailable";return false;}
        const bool value=a.class_is_valuetype(type);
        const auto width=value?static_cast<size_t>(a.class_value_size(type,&align)):sizeof(void*);
        size_t offset=0;if(!layout.offset(a.field_get_offset(field),width,&offset)) {
            *error="[CSharp][ERROR][NativeBridge] API=ValueLayout Stage=BindReferences Expected=FieldWithinValue Actual=Offset:"+
                std::to_string(a.field_get_offset(field))+",Width:"+std::to_string(width)+" Result=Rejected Reason=FieldOutsideValue";return false;
        }
        if(value){if(!valueReferenceOffsets(type,base+offset,out,error,depth+1))return false;}
        else out->push_back(base+offset);
    }
    return true;
}
}
