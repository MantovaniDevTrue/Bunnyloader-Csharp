#pragma once
#include "il2cpp/CheckedProperty.h"

namespace bl::il2cpp {
// Direct field writes are deliberately separate from reads. A property getter
// does not prove that a matching setter exists or that it is transactional.
class CheckedFieldWrite {
    CheckedProperty property;
    bool ready=false;
public:
    const CheckedProperty& contract() const { return property; }
    bool bind(Il2CppClass* owner,const char* name,CheckedProperty::Kind kind,bool stat,
              std::string* error,Il2CppClass* reference=nullptr) {
        ready=false;
        CheckedProperty staged;
        if(!staged.bind(owner,name,kind,stat,error,reference))return false;
        if(!staged.field || (api().field_get_flags(staged.field)&(0x20u|0x40u)))
            return staged.reject(error,"BindWrite","MutableField",staged.field?"ReadOnlyOrLiteral":"GetterOnly","NativeWriteContractMismatch");
        const bool value=api().class_is_valuetype(staged.declared);
        if(value && kind!=CheckedProperty::Kind::Boolean && kind!=CheckedProperty::Kind::Int32 &&
           kind!=CheckedProperty::Kind::Single && kind!=CheckedProperty::Kind::Double)
            return staged.reject(error,"BindWrite","VerifiedPrimitiveOrReference",CheckedProperty::name(staged.declared),"NativeStructuredWriteLayoutNotBound");
        if((stat && !api().field_static_set_value) || (!stat && !value && !api().gc_wbarrier_set_field))
            return staged.reject(error,"BindWrite",stat?"field_static_set_value":"gc_wbarrier_set_field","MissingExport","MetadataUnavailable");
        property=staged;ready=true;return true;
    }
    bool validate(void* self,const void* input,size_t bytes,std::string* error) const {
        if(!ready || !input || bytes!=property.size)
            return property.reject(error,"Write","BoundFieldAndExactValueBytes:"+std::to_string(property.size),
                                   !ready?"Unbound":!input?"NullInput":"Bytes:"+std::to_string(bytes),"NativeWriteContractMismatch");
        auto& a=api();
        if(!property.isStatic && (!self || !a.class_is_assignable_from(property.owner,a.object_get_class(static_cast<Il2CppObject*>(self)))))
            return property.reject(error,"Write","CompatibleOwner","NullOrWrongOwner","NativeObjectMismatch");
        if(!a.class_is_valuetype(property.declared)) {
            Il2CppObject* value=nullptr;std::memcpy(&value,input,sizeof(value));
            if(value && !a.class_is_assignable_from(property.declared,a.object_get_class(value)))
                return property.reject(error,"Write",CheckedProperty::name(property.declared),CheckedProperty::name(a.object_get_class(value)),"NativeReferenceTypeMismatch");
        }
        return true;
    }
    bool write(void* self,const void* input,size_t bytes,std::string* error) const {
        if(!validate(self,input,bytes,error))return false;
        auto& a=api();const bool value=a.class_is_valuetype(property.declared);
        Il2CppObject* reference=nullptr;
        if(!value)std::memcpy(&reference,input,sizeof(reference));
        if(property.isStatic) {
            // IL2CPP takes the reference itself for a reference field, whereas
            // primitive values are passed by address. &reference corrupts it.
            a.field_static_set_value(property.field,value?const_cast<void*>(input):reference);
        } else if(value)std::memcpy(static_cast<char*>(self)+property.offset,input,bytes);
        else a.gc_wbarrier_set_field(static_cast<Il2CppObject*>(self),
                reinterpret_cast<void**>(static_cast<char*>(self)+property.offset),reference);
        return true;
    }
};
}
