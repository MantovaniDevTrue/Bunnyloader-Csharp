#pragma once
#include "experiments/csharp/NativeTooltipBackingPalette.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "il2cpp/Resolver.h"
#include <array>
#include <cstring>
#include <string>
#include <string_view>

namespace bl::csharp {

struct NativeTooltipPanelColorMember {
    Il2CppClass* owner=nullptr;
    Il2CppClass* colorType=nullptr;
    const MethodInfo* getter=nullptr;
    NativeColorLayout layout;
    size_t fieldOffset=0;
    bool isField=false;

    static bool isColor(Il2CppClass* cls) {
        auto& a=il2cpp::api();
        const char* ns=cls&&a.class_get_namespace?a.class_get_namespace(cls):nullptr;
        const char* name=cls&&a.class_get_name?a.class_get_name(cls):nullptr;
        return ns&&name&&!std::strcmp(ns,"Microsoft.Xna.Framework")&&
            !std::strcmp(name,"Color");
    }

    bool bind(Il2CppClass* cls,std::string_view member,std::string* error) {
        using namespace il2cpp;
        *this={};
        auto& a=api();
        if(!error||!cls||member.empty()||member.size()>64||
           !a.class_get_method_from_name||!a.method_is_instance||
           !a.method_get_param_count||!a.method_get_return_type||
           !a.class_from_il2cpp_type||!a.field_get_flags||
           !a.field_get_type||!a.field_get_offset||
           !a.class_instance_size||!a.class_is_assignable_from||
           !a.object_get_class||!a.runtime_invoke)return false;
        const std::string name(member);
        auto* field=findField(cls,name);
        if(field&&!(a.field_get_flags(field)&0x10u)) {
            auto* kind=a.class_from_il2cpp_type(a.field_get_type(field));
            const size_t offset=a.field_get_offset(field);
            if(isColor(kind)&&offset>=sizeof(Il2CppObject)&&
               offset<=a.class_instance_size(cls)&&
               sizeof(uint32_t)<=a.class_instance_size(cls)-offset) {
                NativeColorLayout candidate;
                if(!candidate.bind(kind,error))return false;
                owner=cls;colorType=kind;layout=candidate;fieldOffset=offset;
                isField=true;return true;
            }
        }
        const std::string getterName="get_"+name;
        const auto* method=a.class_get_method_from_name(cls,getterName.c_str(),0);
        if(!method||!a.method_is_instance(method)||a.method_get_param_count(method)!=0) {
            *error="Reason=NativeTooltipPanelColorUnavailable Member="+name;return false;
        }
        auto* type=a.class_from_il2cpp_type(a.method_get_return_type(method));
        if(!isColor(type)) {
            *error="Reason=NativeTooltipPanelColorTypeMismatch Member="+name;return false;
        }
        NativeColorLayout candidate;
        if(!candidate.bind(type,error))return false;
        owner=cls;colorType=type;layout=candidate;getter=method;
        return true;
    }

    bool read(Il2CppObject* object,std::array<uint8_t,4>* color) const {
        auto& a=il2cpp::api();
        if(!object||!color||!owner||!colorType||!a.object_get_class||
           !a.class_is_assignable_from||
           !a.class_is_assignable_from(owner,a.object_get_class(object)))return false;
        std::array<uint8_t,4> bytes{};
        if(isField) {
            if(!a.class_instance_size)return false;
            const auto size=a.class_instance_size(a.object_get_class(object));
            if(fieldOffset>size||bytes.size()>size-fieldOffset)return false;
            std::memcpy(bytes.data(),reinterpret_cast<const uint8_t*>(object)+fieldOffset,bytes.size());
        }else {
            if(!getter||!a.runtime_invoke)return false;
            Il2CppObject* exception=nullptr;
            auto* boxed=a.runtime_invoke(getter,object,nullptr,&exception);
            if(exception||!boxed||a.object_get_class(boxed)!=colorType)return false;
            std::memcpy(bytes.data(),reinterpret_cast<const uint8_t*>(boxed)+sizeof(Il2CppObject),bytes.size());
        }
        std::array<uint8_t,4> rgba{};
        for(size_t i=0;i<4;++i)rgba[i]=bytes[layout.channels[i]];
        *color=rgba;return true;
    }
};

struct NativeTooltipPanelColors {
    NativeTooltipPanelColorMember tint;
    NativeTooltipPanelColorMember border;
    NativeTooltipBackingColorSource tintSource=NativeTooltipBackingColorSource::TooltipBacking;

    bool bind(Il2CppClass* settings,Il2CppClass* backing,
              int context,int loadout,std::string* error) {
        *this={};
        const auto palette=nativeTooltipBackingPalette(context,loadout);
        tintSource=palette.source;
        Il2CppClass* tintOwner=palette.source==NativeTooltipBackingColorSource::Settings?
            settings:backing;
        return tint.bind(tintOwner,palette.name,error)&&
            border.bind(backing,palette.border,error);
    }

    bool read(Il2CppObject* settings,Il2CppObject* backing,
              std::array<uint8_t,4>* selected,std::array<uint8_t,4>* outline) const {
        if(!selected||!outline)return false;
        std::array<uint8_t,4> primary{},stroke{};
        auto* source=tintSource==NativeTooltipBackingColorSource::Settings?settings:backing;
        if(!tint.read(source,&primary)||!border.read(backing,&stroke))return false;
        *selected=primary;*outline=stroke;return true;
    }
};

}
