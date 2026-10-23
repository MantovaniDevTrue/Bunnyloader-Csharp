#pragma once
#include "experiments/csharp/NativeColorLayout.h"
#include "il2cpp/FieldContract.h"
#include <array>
#include <charconv>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace bl::csharp {

struct NativeDrinkColors {
    FieldInfo* field=nullptr;
    Il2CppClass* outerClass=nullptr;
    Il2CppClass* colorClass=nullptr;
    std::array<size_t,4> channels{};
    std::vector<std::array<uint8_t,4>> rgba;
};

struct NativeDrinkColorState {
    uint32_t originalHandle=0;
    uint32_t lastHandle=0;
    std::vector<std::array<uint8_t,4>> lastValues;
};

inline std::map<std::pair<FieldInfo*,int>,NativeDrinkColorState> g_nativeDrinkColors;

inline bool bindNativeDrinkColors(int type,const std::string& encoded,
                                  NativeDrinkColors* out,std::string* error) {
    auto fail=[&](const char* reason) {
        *error=std::string("ItemSet_DrinkParticleColors Reason=")+reason;
        return false;
    };
    auto& a=il2cpp::api();
    if(!a.field_get_type||!a.field_get_flags||!a.class_from_il2cpp_type||
       !a.class_get_rank||!a.class_get_element_class||!a.class_is_valuetype||
       !a.class_get_name||!a.class_get_namespace||!a.class_value_size||
       !a.field_static_get_value||!a.array_new||!a.gchandle_new||
       !a.gchandle_get_target||!a.gchandle_free||!a.gc_wbarrier_set_field)
        return fail("RequiredExportsMissing");
    auto* sets=il2cpp::findClass({"Terraria.ID","ItemID","Sets"});
    if(sets&&a.runtime_class_init)a.runtime_class_init(sets);
    auto* field=sets?il2cpp::findField(sets,"DrinkParticleColors"):nullptr;
    auto* typeInfo=field?a.field_get_type(field):nullptr;
    auto* outer=typeInfo?a.class_from_il2cpp_type(typeInfo):nullptr;
    auto* inner=outer?a.class_get_element_class(outer):nullptr;
    auto* color=inner?a.class_get_element_class(inner):nullptr;
    const char* colorNs=color?a.class_get_namespace(color):nullptr;
    const char* colorName=color?a.class_get_name(color):nullptr;
    uint32_t align=0;
    if(!field||!(a.field_get_flags(field)&0x10u)||!outer||!inner||!color||
       a.class_get_rank(outer)!=1||a.class_get_rank(inner)!=1||
       a.class_get_rank(color)!=0||!a.class_is_valuetype(color)||
       !colorNs||!colorName||std::strcmp(colorNs,"Microsoft.Xna.Framework")||
       std::strcmp(colorName,"Color")||a.class_value_size(color,&align)!=4)
        return fail("ColorJaggedArrayLayoutMismatch");
    NativeColorLayout layout;
    if(!layout.bind(color,error))return false;
    Il2CppArray* array=nullptr;
    a.field_static_get_value(field,&array);
    if(type<1||!array||array->obj.klass!=outer||
       array->length>100000||array->length<=static_cast<uintptr_t>(type))
        return fail("ColorTableCapacityMismatch");
    if(encoded.empty()||encoded.size()>143)return fail("InvalidColorCount");
    NativeDrinkColors pending;
    pending.field=field;pending.outerClass=outer;pending.colorClass=color;
    pending.channels=layout.channels;
    size_t begin=0;
    while(begin<encoded.size()) {
        const size_t end=encoded.find(',',begin);
        const auto token=encoded.substr(begin,end==std::string::npos?std::string::npos:end-begin);
        if(token.size()!=8||pending.rgba.size()>=8)return fail("InvalidColorEncoding");
        uint32_t packed=0;
        const auto parsed=std::from_chars(token.data(),token.data()+token.size(),packed,16);
        if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size())return fail("InvalidColorEncoding");
        pending.rgba.push_back({static_cast<uint8_t>(packed),static_cast<uint8_t>(packed>>8),
            static_cast<uint8_t>(packed>>16),static_cast<uint8_t>(packed>>24)});
        if(end==std::string::npos)break;
        begin=end+1;
        if(begin==encoded.size())return fail("InvalidColorEncoding");
    }
    if(pending.rgba.empty())return fail("InvalidColorCount");
    *out=std::move(pending);
    return true;
}

inline Il2CppArray** nativeDrinkColorCell(const NativeDrinkColors& spec,int type,Il2CppArray** container=nullptr) {
    auto& a=il2cpp::api();
    if(!a.field_static_get_value)return nullptr;
    Il2CppArray* outer=nullptr;
    a.field_static_get_value(spec.field,&outer);
    if(type<1||!outer||outer->obj.klass!=spec.outerClass||
       outer->length>100000||outer->length<=static_cast<uintptr_t>(type))
        return nullptr;
    if(container)*container=outer;
    return reinterpret_cast<Il2CppArray**>(arrayData(outer))+type;
}

inline bool applyNativeDrinkColors(const NativeDrinkColors& spec,int type) {
    auto& a=il2cpp::api();
    Il2CppArray* outer=nullptr;
    auto** cell=nativeDrinkColorCell(spec,type,&outer);
    if(!cell)return false;
    auto key=std::make_pair(spec.field,type);
    auto existing=g_nativeDrinkColors.find(key);
    if(existing!=g_nativeDrinkColors.end()&&existing->second.lastValues==spec.rgba &&
       reinterpret_cast<Il2CppObject*>(*cell)==a.gchandle_get_target(existing->second.lastHandle))
        return true;
    uint32_t original=0;
    if(existing==g_nativeDrinkColors.end()&&*cell) {
        original=a.gchandle_new(reinterpret_cast<Il2CppObject*>(*cell),false);
        if(!original)return false;
    }
    Il2CppArray* inner=a.array_new(spec.colorClass,spec.rgba.size());
    if(!inner||inner->length!=spec.rgba.size()) {
        if(original)a.gchandle_free(original);
        return false;
    }
    auto* data=static_cast<uint8_t*>(arrayData(inner));
    for(size_t i=0;i<spec.rgba.size();++i)
        for(size_t c=0;c<4;++c)
            data[i*4+spec.channels[c]]=spec.rgba[i][c];
    const uint32_t nextHandle=a.gchandle_new(reinterpret_cast<Il2CppObject*>(inner),false);
    if(!nextHandle) {
        if(original)a.gchandle_free(original);
        return false;
    }
    a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(outer),
                            reinterpret_cast<void**>(cell),inner);
    if(existing==g_nativeDrinkColors.end()) {
        NativeDrinkColorState state;
        state.originalHandle=original;state.lastHandle=nextHandle;state.lastValues=spec.rgba;
        g_nativeDrinkColors.emplace(key,std::move(state));
    } else {
        if(existing->second.lastHandle)a.gchandle_free(existing->second.lastHandle);
        existing->second.lastHandle=nextHandle;existing->second.lastValues=spec.rgba;
    }
    return true;
}

inline bool restoreNativeDrinkColors(const NativeDrinkColors& spec,int type) {
    auto key=std::make_pair(spec.field,type);
    auto found=g_nativeDrinkColors.find(key);
    if(found==g_nativeDrinkColors.end())return true;
    auto& a=il2cpp::api();
    Il2CppArray* outer=nullptr;
    auto** cell=nativeDrinkColorCell(spec,type,&outer);
    if(!cell)return false;
    const auto& state=found->second;
    auto* last=a.gchandle_get_target(state.lastHandle);
    if(reinterpret_cast<Il2CppObject*>(*cell)==last) {
        Il2CppObject* previous=state.originalHandle?a.gchandle_get_target(state.originalHandle):nullptr;
        if(state.originalHandle&&!previous)return false;
        a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(outer),
                                reinterpret_cast<void**>(cell),previous);
    }
    if(state.originalHandle)a.gchandle_free(state.originalHandle);
    if(state.lastHandle)a.gchandle_free(state.lastHandle);
    g_nativeDrinkColors.erase(found);
    return true;
}

}
