#pragma once
#include "il2cpp/FieldContract.h"
#include "il2cpp/ValueLayout.h"
#include <initializer_list>

namespace bl::csharp {
// Native detours use Terraria Mobile's IL2CPP ABI. A matching short C# type
// name is insufficient: UnityEngine.Vector2 and XNA Vector2 are distinct types.
inline bool mobileCollisionTypeName(const Il2CppType* type,const char* expected) {
    auto& a=il2cpp::api();
    if(!type||!a.type_get_name||!a.il2cpp_free)return false;
    char* raw=a.type_get_name(type);
    const bool matches=raw&&std::strcmp(raw,expected)==0;
    if(raw)a.il2cpp_free(raw);
    return matches;
}

inline bool mobileCollisionVectorLayout(Il2CppClass* vector,std::string* error) {
    auto& a=il2cpp::api();
    il2cpp::ValueLayout layout;
    if(!vector||!a.field_get_name||!a.class_get_name||!a.class_get_namespace||
       !il2cpp::resolveValueLayout(vector,&layout,error)||layout.size!=8) {
        *error="Reason=MobileVector2LayoutUnavailable";return false;
    }
    const char* name=a.class_get_name(vector),*ns=a.class_get_namespace(vector);
    if(!name||!ns||std::strcmp(name,"Vector2")||std::strcmp(ns,"Microsoft.Xna.Framework")) {
        *error="Reason=MobileVector2TypeMismatch";return false;
    }
    bool x=false,y=false;size_t count=0;void* iterator=nullptr;
    while(auto* field=a.class_get_fields(vector,&iterator)) {
        if(a.field_get_flags(field)&0x10u)continue;
        ++count;size_t offset=0;
        auto* type=a.class_from_il2cpp_type(a.field_get_type(field));uint32_t align=0;
        const char* fieldName=a.field_get_name(field);
        const char* fieldType=type?a.class_get_name(type):nullptr;
        const char* fieldNs=type?a.class_get_namespace(type):nullptr;
        if(!fieldName||!fieldType||!fieldNs||std::strcmp(fieldType,"Single")||
           std::strcmp(fieldNs,"System")||!a.class_is_valuetype(type)||
           a.class_value_size(type,&align)!=4||!layout.offset(a.field_get_offset(field),4,&offset)) {
            *error="Reason=MobileVector2FieldMismatch";return false;
        }
        if(!std::strcmp(fieldName,"X")&&offset==0)x=true;
        else if(!std::strcmp(fieldName,"Y")&&offset==4)y=true;
        else {*error="Reason=MobileVector2FieldOrderMismatch";return false;}
    }
    if(count!=2||!x||!y){*error="Reason=MobileVector2FieldsIncomplete";return false;}
    return true;
}

inline bool mobileCollisionMethod(Il2CppClass* projectile,const MethodInfo* method,
                                  std::initializer_list<const char*> parameters,
                                  std::string* error) {
    auto& a=il2cpp::api();
    if(!projectile||!method||!a.method_is_instance||!a.method_is_generic||
       !a.method_get_class||!a.method_get_return_type||!a.method_get_param_count||
       !a.method_get_param||!a.type_get_name||!a.il2cpp_free) {
        *error="Reason=MobileCollisionMethodMetadataUnavailable";return false;
    }
    if(!a.method_is_instance(method)||a.method_is_generic(method)||
       a.method_get_class(method)!=projectile||
       !mobileCollisionTypeName(a.method_get_return_type(method),"System.Void")||
       a.method_get_param_count(method)!=parameters.size()) {
        *error="Reason=MobileCollisionMethodAbiMismatch";return false;
    }
    uint32_t index=0;
    for(const auto* parameter:parameters) {
        if(!mobileCollisionTypeName(a.method_get_param(method,index),parameter)) {
            *error="Reason=MobileCollisionParameterAbiMismatch Index="+std::to_string(index);return false;
        }
        ++index;
    }
    return true;
}

inline bool validateMobileTileCollisionMethods(Il2CppClass* projectile,
    const MethodInfo* movement,const MethodInfo* kill,const MethodInfo* params,
    bool requireParams,std::string* error) {
    auto& a=il2cpp::api();
    if(!error)return false;
    if(!mobileCollisionMethod(projectile,movement,{"Microsoft.Xna.Framework.Vector2"},error)||
       !mobileCollisionMethod(projectile,kill,{},error))return false;
    if(!a.class_from_il2cpp_type) {
        *error="Reason=MobileCollisionTypeMetadataUnavailable";return false;
    }
    if(!mobileCollisionVectorLayout(a.class_from_il2cpp_type(a.method_get_param(movement,0)),error))return false;
    // The three out parameters are pointers in the ARM64 detour. A value
    // parameter of the same short name would use different argument registers.
    if(requireParams&&!mobileCollisionMethod(projectile,params,
        {"Microsoft.Xna.Framework.Vector2&","System.Int32&","System.Int32&"},error))return false;
    error->clear();return true;
}

inline bool mobileTileCollisionNetMode(Il2CppClass* main,FieldInfo** field,std::string* error) {
    if(!error)return false;
    if(!il2cpp::api().field_static_get_value) {
        *error="Field=netMode Reason=MobileStaticReadUnavailable";return false;
    }
    // Mobile 301720 declares static System.Int32. Never read four bytes just
    // because a field with this name exists in another Terraria build.
    return il2cpp::nativeFieldContract(main,"netMode","System.Int32",true,4,field,nullptr,error);
}
}
