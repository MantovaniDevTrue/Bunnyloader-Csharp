#pragma once
#include "experiments/csharp/NativeColorLayout.h"
#include "il2cpp/Resolver.h"
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace bl::csharp {

struct NativeTooltipPanelAlpha {
    const MethodInfo* multiply=nullptr;
    Il2CppClass* colorType=nullptr;
    NativeColorLayout channels;

    bool bind(Il2CppClass* cls,std::string* error) {
        using namespace il2cpp;
        *this={};
        if(!error)return false;
        auto& a=api();
        if(!cls||!a.class_get_namespace||!a.class_get_name||
           !a.class_get_methods||!a.method_get_name||!a.method_get_param_count||
           !a.method_get_param||!a.method_get_return_type||
           !a.method_is_instance||!a.class_from_il2cpp_type||
           !a.object_get_class||!a.runtime_invoke)
            return false;
        const char* ns=a.class_get_namespace(cls);
        const char* name=a.class_get_name(cls);
        if(!ns||!name||std::strcmp(ns,"Microsoft.Xna.Framework")||
           std::strcmp(name,"Color")) {
            *error="Reason=TooltipAlphaColorTypeUnknown";return false;
        }
        const MethodInfo* resolved=nullptr;
        void* iterator=nullptr;
        while(auto* candidate=a.class_get_methods(cls,&iterator)) {
            const char* member=a.method_get_name(candidate);
            if(!member||std::strcmp(member,"Multiply")||
               a.method_get_param_count(candidate)!=2)continue;
            if(resolved) {
                *error="Reason=TooltipAlphaMultiplyOverloadAmbiguous";return false;
            }
            resolved=candidate;
        }
        if(!resolved||a.method_is_instance(resolved)) {
            *error="Reason=TooltipAlphaMultiplyUnavailable";return false;
        }
        auto* left=a.class_from_il2cpp_type(a.method_get_param(resolved,0));
        auto* scale=a.class_from_il2cpp_type(a.method_get_param(resolved,1));
        auto* returned=a.class_from_il2cpp_type(a.method_get_return_type(resolved));
        if(left!=cls||returned!=cls||!scale||
           !a.class_get_name||!a.class_get_namespace(scale)||
           std::strcmp(a.class_get_namespace(scale),"System")||
           std::strcmp(a.class_get_name(scale),"Single")) {
            *error="Reason=TooltipAlphaMultiplySignatureMismatch";return false;
        }
        NativeColorLayout order;
        if(!order.bind(cls,error))return false;
        multiply=resolved;colorType=cls;channels=order;
        return true;
    }

    bool apply(const std::array<uint8_t,4>& rgba,float opacity,
               std::array<uint8_t,4>* result) const {
        auto& a=il2cpp::api();
        if(!multiply||!colorType||!result||!a.runtime_invoke||
           !a.object_get_class||!std::isfinite(opacity)||opacity<0.f||opacity>1.f)
            return false;
        uint32_t packed=channels.pack(rgba[0],rgba[1],rgba[2],rgba[3]);
        void* params[]={&packed,&opacity};
        Il2CppObject* exception=nullptr;
        auto* output=a.runtime_invoke(multiply,nullptr,params,&exception);
        if(exception||!output||a.object_get_class(output)!=colorType)return false;
        uint8_t bytes[4]{};
        std::memcpy(bytes,reinterpret_cast<const uint8_t*>(output)+sizeof(Il2CppObject),4);
        std::array<uint8_t,4> converted{};
        for(size_t i=0;i<4;++i)converted[i]=bytes[channels.channels[i]];
        *result=converted;return true;
    }
};

}
