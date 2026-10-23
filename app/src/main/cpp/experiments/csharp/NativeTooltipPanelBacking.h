#pragma once
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/NativeTooltipPanelLayout.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <string_view>

namespace bl::csharp {

struct NativeTooltipPanelBacking {
    const MethodInfo* method=nullptr;
    Il2CppClass* textureType=nullptr;
    NativeColorLayout colorLayout;
    size_t nullableBytes=0;

    static bool typeIs(Il2CppClass* cls,std::string_view ns,std::string_view name) {
        auto& a=il2cpp::api();
        if(!cls||!a.class_get_name||!a.class_get_namespace)return false;
        const char* n=a.class_get_name(cls),*space=a.class_get_namespace(cls);
        return n&&space&&ns==space&&name==n;
    }

    static bool vector2(Il2CppClass* cls,std::string* error) {
        using namespace il2cpp;
        auto& a=api();
        ValueLayout layout;
        if(!typeIs(cls,"Microsoft.Xna.Framework","Vector2")||
           !resolveValueLayout(cls,&layout,error)||layout.size!=8)return false;
        for(const auto& [name,offset]:{std::pair{"X",size_t{0}},std::pair{"Y",size_t{4}}}) {
            auto* field=findField(cls,name);
            if(!field||!a.field_get_type||!a.field_get_flags||!a.field_get_offset||
               (a.field_get_flags(field)&0x10u)||
               !typeIs(a.class_from_il2cpp_type(a.field_get_type(field)),"System","Single"))
                return false;
            size_t found=0;
            if(!layout.offset(a.field_get_offset(field),4,&found)||found!=offset)return false;
        }
        return true;
    }

    static bool rectangle(Il2CppClass* cls,std::string* error) {
        using namespace il2cpp;
        auto& a=api();
        ValueLayout layout;
        if(!typeIs(cls,"Microsoft.Xna.Framework","Rectangle")||
           !resolveValueLayout(cls,&layout,error)||layout.size!=16)return false;
        const char* fields[]={"X","Y","Width","Height"};
        for(size_t i=0;i<4;++i) {
            auto* field=findField(cls,fields[i]);
            if(!field||!a.field_get_type||!a.field_get_flags||!a.field_get_offset||
               (a.field_get_flags(field)&0x10u)||
               !typeIs(a.class_from_il2cpp_type(a.field_get_type(field)),"System","Int32"))
                return false;
            size_t found=0;
            if(!layout.offset(a.field_get_offset(field),4,&found)||found!=i*4)return false;
        }
        return true;
    }

    static bool nullableRectangle(Il2CppClass* cls,size_t* bytes,std::string* error) {
        using namespace il2cpp;
        auto& a=api();
        ValueLayout layout;
        if(!bytes||!typeIs(cls,"System","Nullable`1")||
           !resolveValueLayout(cls,&layout,error)||layout.size!=20)return false;
        auto* flag=findField(cls,"hasValue");
        auto* value=findField(cls,"value");
        if(!flag||!value||!a.field_get_flags||!a.field_get_type||!a.field_get_offset||
           (a.field_get_flags(flag)&0x10u)||(a.field_get_flags(value)&0x10u)||
           !typeIs(a.class_from_il2cpp_type(a.field_get_type(flag)),"System","Boolean"))
            return false;
        size_t flagAt=0,rectAt=0;
        if(!layout.offset(a.field_get_offset(flag),1,&flagAt)||flagAt!=0||
           !layout.offset(a.field_get_offset(value),16,&rectAt)||rectAt!=4||
           !rectangle(a.class_from_il2cpp_type(a.field_get_type(value)),error))
            return false;
        *bytes=layout.size;
        return true;
    }

    bool bind(Il2CppClass* panel,std::string* error) {
        using namespace il2cpp;
        auto& a=api();
        if(!error)return false;
        auto fail=[&](const char* reason){*error=std::string("Reason=")+reason;return false;};
        if(!panel||!a.class_get_methods||!a.method_get_name||!a.method_get_param_count||
           !a.method_get_param||!a.method_get_param_name||!a.method_get_return_type||
           !a.method_is_instance||!a.class_from_il2cpp_type||!a.runtime_invoke||
           !a.object_get_class||!a.class_is_assignable_from||!a.class_is_valuetype)
            return fail("NativeTooltipBackingMetadataUnavailable");
        if(!typeIs(panel,"","GUIPanel"))
            return fail("NativeTooltipBackingOwnerUnknown");
        const MethodInfo* found=nullptr;
        void* iterator=nullptr;
        while(auto* candidate=a.class_get_methods(panel,&iterator)) {
            const char* name=a.method_get_name(candidate);
            if(!name||std::strcmp(name,"DrawBacking")!=0||
               a.method_get_param_count(candidate)!=5)continue;
            if(found)return fail("NativeTooltipBackingOverloadAmbiguous");
            found=candidate;
        }
        if(!found||a.method_is_instance(found))return fail("NativeTooltipBackingMethodUnavailable");
        auto* ret=a.class_from_il2cpp_type(a.method_get_return_type(found));
        if(!typeIs(ret,"System","Void"))return fail("NativeTooltipBackingReturnMismatch");
        const char* names[]={"position","size","texture","color","sourceFrame"};
        std::array<Il2CppClass*,5> types{};
        for(size_t i=0;i<5;++i) {
            const char* label=a.method_get_param_name(found,static_cast<uint32_t>(i));
            types[i]=a.class_from_il2cpp_type(a.method_get_param(found,static_cast<uint32_t>(i)));
            if(!label||std::strcmp(label,names[i])||!types[i])
                return fail("NativeTooltipBackingParameterMismatch");
        }
        if(types[0]!=types[1]||!vector2(types[0],error)||
           !typeIs(types[2],"Microsoft.Xna.Framework.Graphics","Texture2D")||
           a.class_is_valuetype(types[2])||
           !typeIs(types[3],"Microsoft.Xna.Framework","Color")||
           !typeIs(types[4],"System","Nullable`1"))
            return fail("NativeTooltipBackingTypeMismatch");
        NativeColorLayout channels;
        if(!channels.bind(types[3],error))return false;
        size_t nullableSize=0;
        if(!nullableRectangle(types[4],&nullableSize,error))
            return fail("NativeTooltipBackingNullableRectangleMismatch");
        method=found;textureType=types[2];colorLayout=channels;nullableBytes=nullableSize;
        return true;
    }

    bool draw(const NativeTooltipPanelResult& panel,Il2CppObject* texture,
              uint8_t r,uint8_t g,uint8_t b,uint8_t alpha) const {
        auto& a=il2cpp::api();
        if(!method||!textureType||!texture||!a.runtime_invoke||!a.object_get_class||
           !a.class_is_assignable_from||!a.class_is_assignable_from(textureType,a.object_get_class(texture))||
           nullableBytes!=20||!panel.opaque||
           !std::isfinite(panel.panelX)||!std::isfinite(panel.panelY)||
           !std::isfinite(panel.panelWidth)||!std::isfinite(panel.panelHeight)||
           panel.panelWidth<=0||panel.panelHeight<=0)return false;
        const std::array<float,2> pos{panel.panelX,panel.panelY};
        const std::array<float,2> size{panel.panelWidth,panel.panelHeight};
        const uint32_t rgba=colorLayout.pack(r,g,b,alpha);
        std::array<uint8_t,20> emptyNullable{};
        void* args[]={const_cast<float*>(pos.data()),const_cast<float*>(size.data()),
                      texture,const_cast<uint32_t*>(&rgba),emptyNullable.data()};
        Il2CppObject* exception=nullptr;
        a.runtime_invoke(method,nullptr,args,&exception);
        return exception==nullptr;
    }
};

}
