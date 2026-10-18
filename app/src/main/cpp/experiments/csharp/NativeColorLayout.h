#pragma once
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include <array>

namespace bl::csharp {
// Color's namespace and byte order differ between runtime builds. Bind the
// actual value type supplied by the method/field, never a managed stand-in.
struct NativeColorLayout {
    std::array<size_t,4> channels{}; // R, G, B, A offsets inside the value
    bool bind(Il2CppClass* type,std::string* error) {
        using namespace il2cpp;
        auto& a=api();ValueLayout layout;
        if(!resolveValueLayout(type,&layout,error)||layout.size!=4){*error="Reason=ColorAbiMismatch";return false;}
        const char* names[]={"R","G","B","A"};
        std::array<bool,4> used{};
        for(size_t i=0;i<4;++i) {
            auto* field=findField(type,names[i]);
            auto* channel=field?a.class_from_il2cpp_type(a.field_get_type(field)):nullptr;
            uint32_t align=0;size_t offset=0;
            const char* ns=channel&&a.class_get_namespace?a.class_get_namespace(channel):nullptr;
            const char* name=channel&&a.class_get_name?a.class_get_name(channel):nullptr;
            if(!field||(a.field_get_flags(field)&0x10u)||!ns||!name||std::strcmp(ns,"System")||std::strcmp(name,"Byte")||
               !a.class_is_valuetype(channel)||a.class_value_size(channel,&align)!=1||
               !layout.offset(a.field_get_offset(field),1,&offset)||used[offset]) {
                *error=std::string("Reason=ColorChannelLayoutUnavailable Field=")+names[i];return false;
            }
            channels[i]=offset;used[offset]=true;
        }
        return true;
    }
    uint32_t pack(uint8_t r,uint8_t g,uint8_t b,uint8_t alpha) const {
        std::array<uint8_t,4> bytes{};
        bytes[channels[0]]=r;bytes[channels[1]]=g;bytes[channels[2]]=b;bytes[channels[3]]=alpha;
        uint32_t value=0;std::memcpy(&value,bytes.data(),sizeof(value));return value;
    }
    uint32_t fromArgb(uint32_t value) const {
        return pack(static_cast<uint8_t>(value>>16),static_cast<uint8_t>(value>>8),
                    static_cast<uint8_t>(value),static_cast<uint8_t>(value>>24));
    }
};
}
