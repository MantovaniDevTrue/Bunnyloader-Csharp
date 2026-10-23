#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cstring>
#include <string>
#include <string_view>

namespace bl::csharp {

struct NativeTooltipPanelObjectMember {
    Il2CppClass* owner=nullptr;
    Il2CppClass* type=nullptr;
    const MethodInfo* getter=nullptr;
    size_t fieldOffset=0;
    bool isField=false;

    bool bind(Il2CppClass* cls,std::string_view member,std::string* error) {
        using namespace il2cpp;
        *this={};
        auto& a=api();
        if(!error||!cls||member.empty()||member.size()>64||
           !a.class_get_name||!a.class_is_valuetype||!a.class_get_method_from_name||
           !a.method_is_instance||!a.method_get_return_type||
           !a.method_get_param_count||!a.class_from_il2cpp_type||
           !a.class_is_assignable_from||!a.object_get_class||
           !a.runtime_invoke)return false;
        const std::string name(member);
        auto* field=findField(cls,name);
        if(field&&a.field_get_flags&&a.field_get_type&&a.field_get_offset&&
           a.class_instance_size&&
           !(a.field_get_flags(field)&0x10u)) {
            auto* found=a.class_from_il2cpp_type(a.field_get_type(field));
            const auto offset=a.field_get_offset(field);
            if(found&&!a.class_is_valuetype(found)&&
               offset>=sizeof(Il2CppObject)&&
               offset+sizeof(Il2CppObject*)<=a.class_instance_size(cls)) {
                owner=cls;type=found;fieldOffset=offset;isField=true;
                return true;
            }
        }
        const std::string getterName="get_"+name;
        auto* method=a.class_get_method_from_name(cls,getterName.c_str(),0);
        if(!method||!a.method_is_instance(method)||a.method_get_param_count(method)!=0||
           !a.method_get_return_type(method)) {
            *error="Reason=NativeTooltipPanelReferenceUnavailable Member="+name;return false;
        }
        auto* found=a.class_from_il2cpp_type(a.method_get_return_type(method));
        if(!found||a.class_is_valuetype(found)) {
            *error="Reason=NativeTooltipPanelReferenceIsValue Member="+name;return false;
        }
        owner=cls;type=found;getter=method;
        return true;
    }

    bool read(Il2CppObject* parent,Il2CppObject** out) const {
        auto& a=il2cpp::api();
        if(!out||!parent||!owner||!type||!a.object_get_class||
           !a.class_is_assignable_from||
           !a.class_is_assignable_from(owner,a.object_get_class(parent)))return false;
        Il2CppObject* value=nullptr;
        if(isField) {
            if(!a.class_instance_size||
               fieldOffset+sizeof(value)>a.class_instance_size(a.object_get_class(parent)))
                return false;
            std::memcpy(&value,reinterpret_cast<const uint8_t*>(parent)+fieldOffset,sizeof(value));
        } else {
            if(!getter||!a.runtime_invoke)return false;
            Il2CppObject* error=nullptr;
            value=a.runtime_invoke(getter,parent,nullptr,&error);
            if(error)return false;
        }
        if(!value||!a.class_is_assignable_from(type,a.object_get_class(value)))
            return false;
        *out=value;return true;
    }
};

struct NativeTooltipPanelTexturePath {
    NativeTooltipPanelObjectMember tooltips;
    NativeTooltipPanelObjectMember backing;
    NativeTooltipPanelObjectMember fill;
    NativeTooltipPanelObjectMember border;
    NativeTooltipPanelObjectMember fillTexture;
    NativeTooltipPanelObjectMember borderTexture;

    bool bind(Il2CppClass* settings,std::string* error) {
        *this={};
        if(!tooltips.bind(settings,"Tooltips",error)||
           !backing.bind(tooltips.type,"Backing",error)||
           !fill.bind(backing.type,"Backing",error)||
           !border.bind(backing.type,"Border",error)||
           !fillTexture.bind(fill.type,"Texture",error)||
           !borderTexture.bind(border.type,"Texture",error))
            return false;
        auto& a=il2cpp::api();
        auto isTexture=[&](Il2CppClass* cls){
            const char* ns=a.class_get_namespace?a.class_get_namespace(cls):nullptr;
            const char* name=a.class_get_name?a.class_get_name(cls):nullptr;
            return ns&&name&&!std::strcmp(ns,"Microsoft.Xna.Framework.Graphics")&&
                !std::strcmp(name,"Texture2D");
        };
        if(!isTexture(fillTexture.type)||!isTexture(borderTexture.type)){
            if(error)*error="Reason=NativeTooltipPanelTextureTypeMismatch";
            return false;
        }
        return true;
    }

    bool read(Il2CppObject* settings,Il2CppObject** fillOut,
              Il2CppObject** borderOut) const {
        if(!fillOut||!borderOut)return false;
        Il2CppObject *tips=nullptr,*back=nullptr,*fillObj=nullptr,*borderObj=nullptr;
        Il2CppObject *fillValue=nullptr,*borderValue=nullptr;
        if(!tooltips.read(settings,&tips)||!backing.read(tips,&back)||
           !fill.read(back,&fillObj)||!border.read(back,&borderObj)||
           !fillTexture.read(fillObj,&fillValue)||
           !borderTexture.read(borderObj,&borderValue))
            return false;
        *fillOut=fillValue;*borderOut=borderValue;
        return true;
    }
};

}
