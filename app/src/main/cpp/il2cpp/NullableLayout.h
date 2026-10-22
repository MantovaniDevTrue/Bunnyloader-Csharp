#pragma once
#include "il2cpp/CheckedProperty.h"
#include "il2cpp/ValueLayout.h"
#include <array>

namespace bl::il2cpp {
// runtime_invoke receives the verified closed generic value. No assumption is
// made about field order, boxed offsets, padding, or the size of Nullable<T>.
class NullableLayout {
    ValueLayout layout;
    size_t flagOffset=0,valueOffset=0,valueBytes=0;
    bool ready=false;
public:
    static constexpr size_t Capacity=80;
    struct alignas(16) Buffer:std::array<unsigned char,Capacity> {};
    size_t size() const {return ready?layout.size:0;}
    bool bind(Il2CppClass* nullable,Il2CppClass* value,std::string* error) {
        ready=false;auto& a=api();CheckedProperty diagnostic;diagnostic.label=CheckedProperty::name(nullable);
        auto fail=[&](const std::string& actual,const char* reason){return diagnostic.reject(error,"BindNullable","ClosedSystem.Nullable<T>:BooleanAndExactT",actual,reason);};
        if(!nullable || !value || !a.class_get_fields || !a.field_get_type || !a.field_get_flags || !a.field_get_offset ||
           !a.field_get_name || !a.class_from_il2cpp_type || !a.class_is_valuetype || !a.class_value_size ||
           !a.class_get_name || !a.class_get_namespace || !a.type_get_name || !a.il2cpp_free)
            return fail("MissingTypeOrExports","MetadataUnavailable");
        if(CheckedProperty::name(nullable)!="System.Nullable`1" || !a.class_is_valuetype(value))
            return fail(CheckedProperty::name(nullable)+"<"+CheckedProperty::name(value)+">","NativeGenericContractMismatch");
        ValueLayout candidate;if(!resolveValueLayout(nullable,&candidate,error))return false;
        if(candidate.size>Capacity)return fail("Bytes:"+std::to_string(candidate.size),"NativeValueCapacityExceeded");
        uint32_t align=0;a.class_value_size(nullable,&align);
        if(align>16)return fail("Alignment:"+std::to_string(align),"NativeValueAlignmentMismatch");
        const int bytes=a.class_value_size(value,&align);
        if(bytes<=0 || size_t(bytes)>Capacity || align>16)return fail("ValueBytes:"+std::to_string(bytes)+",Alignment:"+std::to_string(align),"NativeValueSizeMismatch");
        std::vector<size_t> references;
        if(!valueReferenceOffsets(value,0,&references,error))return false;
        if(!references.empty())return fail("ValueContainsManagedReferences","NativeNullableReferenceValueUnsupported");
        size_t count=0,flag=0,data=0;bool hasFlag=false,hasData=false;void* it=nullptr;
        while(auto* field=a.class_get_fields(nullable,&it)) {
            if(a.field_get_flags(field)&0x10u)continue;
            ++count;const char* name=a.field_get_name(field);
            const auto* fieldType=a.field_get_type(field);
            auto* cls=fieldType?a.class_from_il2cpp_type(fieldType):nullptr;
            char* typeName=fieldType && a.type_get_name?a.type_get_name(fieldType):nullptr;
            if(!typeName)return fail(name?name:"Unnamed","NativeTypeNameUnavailable");
            const bool indirect=typeName && (std::strchr(typeName,'&') || std::strchr(typeName,'*'));
            if(typeName && a.il2cpp_free)a.il2cpp_free(typeName);
            if(indirect)return fail(name?name:"Unnamed","NativeIndirectionMismatch");
            if(name && std::strcmp(name,"hasValue")==0) {
                if(CheckedProperty::name(cls)!="System.Boolean" || !a.class_is_valuetype(cls) || a.class_value_size(cls,&align)!=1 ||
                   !candidate.offset(a.field_get_offset(field),1,&flag))return fail("hasValue:"+CheckedProperty::name(cls),"NativeNullableFlagMismatch");
                hasFlag=true;
            } else if(name && std::strcmp(name,"value")==0) {
                if(cls!=value || !candidate.offset(a.field_get_offset(field),size_t(bytes),&data))
                    return fail("value:"+CheckedProperty::name(cls),"NativeNullableValueMismatch");
                hasData=true;
            } else return fail(name?name:"Unnamed","UnexpectedNativeNullableField");
        }
        if(count!=2 || !hasFlag || !hasData || (flag>=data && flag<data+size_t(bytes)))
            return fail("Fields:"+std::to_string(count)+",FlagOffset:"+std::to_string(flag)+",ValueOffset:"+std::to_string(data),"NativeNullableLayoutMismatch");
        layout=candidate;flagOffset=flag;valueOffset=data;valueBytes=size_t(bytes);ready=true;return true;
    }
    bool encode(bool present,const void* value,size_t bytes,Buffer* out,std::string* error) const {
        if(!ready || !out || bytes!=valueBytes || (present && !value)) {
            if(error)*error="[Native][ERROR][Nullable] Stage=Encode Result=Rejected Reason=UnboundOrValueSizeMismatch Expected="+
                std::to_string(valueBytes)+" Actual="+std::to_string(bytes);
            return false;
        }
        out->fill(0);(*out)[flagOffset]=present?1:0;
        if(present)std::memcpy(out->data()+valueOffset,value,valueBytes);
        return true;
    }
    bool decode(const void* input,size_t bytes,bool* present,void* value,size_t capacity,std::string* error) const {
        if(!ready || !input || !present || !value || bytes!=layout.size || capacity!=valueBytes) {
            if(error)*error="[Native][ERROR][Nullable] Stage=Decode Result=Rejected Reason=UnboundOrValueSizeMismatch";
            return false;
        }
        const auto* data=static_cast<const unsigned char*>(input);
        if(data[flagOffset]>1) {
            if(error)*error="[Native][ERROR][Nullable] Stage=Decode Expected=Boolean:0Or1 Actual="+std::to_string(data[flagOffset])+" Result=Rejected Reason=InvalidNativeNullableFlag";
            return false;
        }
        *present=data[flagOffset]!=0;std::memset(value,0,valueBytes);
        if(*present)std::memcpy(value,data+valueOffset,valueBytes);
        return true;
    }
};
}
