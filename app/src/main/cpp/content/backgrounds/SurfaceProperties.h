#pragma once
#include "il2cpp/CheckedProperty.h"
#include "il2cpp/CheckedFieldWrite.h"
#include "il2cpp/ValueLayout.h"

namespace bl::runtime::surface {
// Android exposes some drawing state through getters. Bind exact numeric
// widths once, rather than reinterpreting every scalar as an Int32 or pointer.
struct NumberProperty {
    il2cpp::CheckedProperty property;
    bool bind(Il2CppClass* cls,const char* name,bool stat,std::string* error) {
        using K=il2cpp::CheckedProperty::Kind;
        std::string detail;
        for(auto kind:{K::Int32,K::Single,K::Double})if(property.bind(cls,name,kind,stat,&detail))return true;
        if(error)*error=detail+" Allowed=System.Int32:4,System.Single:4,System.Double:8";
        return false;
    }
    bool read(void* self,double* value,std::string* error) const {
        using K=il2cpp::CheckedProperty::Kind;
        if(property.kind==K::Int32){int32_t v=0;if(!property.read(self,&v,error))return false;*value=v;}
        else if(property.kind==K::Single){float v=0;if(!property.read(self,&v,error))return false;*value=v;}
        else return property.read(self,value,error);
        return true;
    }
};
// A setter is verified separately from its getter. Only one scalar/reference
// is written here; multi-table publication still requires direct fields.
struct PropertyWrite {
    il2cpp::CheckedProperty property;
    const MethodInfo* setter=nullptr;
    bool bind(Il2CppClass* cls,const char* name,il2cpp::CheckedProperty::Kind kind,bool stat,std::string* error,Il2CppClass* reference=nullptr) {
        auto& a=il2cpp::api();setter=nullptr;
        if(!property.bind(cls,name,kind,stat,error,reference))return false;
        if(property.field) {
            if((a.field_get_flags(property.field)&(0x20u|0x40u)) || (stat && !a.field_static_set_value) ||
               (!stat && !a.class_is_valuetype(property.declared) && !a.gc_wbarrier_set_field))
                return property.reject(error,"BindWrite","MutableFieldAndWriteExports","ReadOnlyOrMissingExport","NativeWriteContractMismatch");
            return true;
        }
        if(!a.class_get_methods || !a.method_get_name || !a.method_is_generic || !a.type_get_name || !a.il2cpp_free)
            return property.reject(error,"BindSetter","SetterEnumerationAndTypeExports","Missing","MetadataUnavailable");
        const std::string method="set_"+std::string(name);void* it=nullptr;
        while(const auto* m=a.class_get_methods(cls,&it)) {
            const char* n=a.method_get_name(m);
            if(!n || method!=n || a.method_get_param_count(m)!=1 || a.method_is_generic(m) ||
               bool(a.method_get_flags(m,nullptr)&0x10u)!=stat || (a.method_get_flags(m,nullptr)&0x400u))continue;
            auto* value=a.method_get_param(m,0);auto* result=a.method_get_return_type(m);
            char* type=value?a.type_get_name(value):nullptr;char* returned=result?a.type_get_name(result):nullptr;
            const bool exact=type && returned && !std::strchr(type,'&') && !std::strchr(type,'*') &&
                !std::strchr(returned,'&') && !std::strchr(returned,'*') &&
                a.class_from_il2cpp_type(value)==property.declared &&
                il2cpp::CheckedProperty::name(a.class_from_il2cpp_type(result))=="System.Void";
            if(type)a.il2cpp_free(type);
            if(returned)a.il2cpp_free(returned);
            if(!exact)continue;
            if(setter)return property.reject(error,"BindSetter","OneExactSetter","MultipleMatchingMethods","AmbiguousNativeSetter");
            setter=m;
        }
        return setter || property.reject(error,"BindSetter","VoidSetterWithExactGetterTypeAndStorage","MissingOrIncompatible","NativeSetterContractMismatch");
    }
    bool write(void* self,const void* value,size_t bytes,std::string* error) const {
        auto& a=il2cpp::api();
        if(!value || bytes!=property.size || (!property.isStatic && (!self || !a.class_is_assignable_from(property.owner,a.object_get_class(static_cast<Il2CppObject*>(self))))))
            return property.reject(error,"Write","ExactValueBytesAndCompatibleOwner",std::to_string(bytes),"NativeWriteContractMismatch");
        const bool scalar=a.class_is_valuetype(property.declared);
        Il2CppObject* object=nullptr;if(!scalar)std::memcpy(&object,value,sizeof(object));
        if(!scalar && object && !a.class_is_assignable_from(property.declared,a.object_get_class(object)))
            return property.reject(error,"Write",il2cpp::CheckedProperty::name(property.declared),il2cpp::CheckedProperty::name(a.object_get_class(object)),"NativeReferenceTypeMismatch");
        if(setter) {
            void* args[]={scalar?const_cast<void*>(value):object};Il2CppObject* exception=nullptr;
            a.runtime_invoke(setter,property.isStatic?nullptr:self,args,&exception);
            if(exception) {
                char detail[2048]{};if(a.format_exception)a.format_exception(exception,detail,sizeof(detail));detail[sizeof(detail)-1]=0;
                return property.reject(error,"Setter","NoException",detail[0]?detail:il2cpp::CheckedProperty::name(a.object_get_class(exception)),"NativeSetterException");
            }
        } else if(property.field) {
            if(property.isStatic)a.field_static_set_value(property.field,scalar?const_cast<void*>(value):object);
            else if(scalar)std::memcpy(static_cast<char*>(self)+property.offset,value,bytes);
            else a.gc_wbarrier_set_field(static_cast<Il2CppObject*>(self),reinterpret_cast<void**>(static_cast<char*>(self)+property.offset),object);
        } else return property.reject(error,"Write","BoundFieldOrSetter","Unbound","NativeWriteContractMismatch");
        return true;
    }
};
}
