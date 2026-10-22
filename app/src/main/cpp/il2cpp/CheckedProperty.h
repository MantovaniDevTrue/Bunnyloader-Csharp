#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cstring>
#include <string>

namespace bl::il2cpp {
// A cached read contract: both fields and property getters are common in the
// Android build. Never read a scalar as an object pointer to inspect its type.
class CheckedProperty {
public:
    enum class Kind { Boolean, Int32, Single, Double, Vector2, IntArray, SingleArray, BooleanArray, TextureArray, AssetArray, Reference };
    FieldInfo* field=nullptr;
    const MethodInfo* getter=nullptr;
    Il2CppClass* declared=nullptr;
    Il2CppClass* owner=nullptr;
    Kind kind=Kind::Reference;
    bool isStatic=true;
    size_t offset=0,size=0;
    std::string label;
    bool reject(std::string* error,const char* stage,const std::string& expected,const std::string& actual,const char* reason) const {
        if(error)*error="[Native][ERROR][Property] Field="+label+" Stage="+stage+" Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;
        return false;
    }
    static std::string name(Il2CppClass* c) {
        auto& a=api();if(!c)return "Missing";
        const char* ns=a.class_get_namespace?a.class_get_namespace(c):nullptr,*n=a.class_get_name?a.class_get_name(c):nullptr;
        return std::string(ns?ns:"?")+"."+(n?n:"?");
    }
    static const char* expectedName(Kind expected) {
        switch(expected) {
        case Kind::Boolean:return "System.Boolean:1";
        case Kind::Int32:return "System.Int32:4";
        case Kind::Single:return "System.Single:4";
        case Kind::Double:return "System.Double:8";
        case Kind::Vector2:return "Microsoft.Xna.Framework.Vector2:8";
        case Kind::IntArray:return "System.Int32[]:Rank1";
        case Kind::SingleArray:return "System.Single[]:Rank1";
        case Kind::BooleanArray:return "System.Boolean[]:Rank1";
        case Kind::TextureArray:return "Texture2D[]:Rank1";
        case Kind::AssetArray:return "Asset<Texture2D>[]:Rank1";
        case Kind::Reference:return "CompatibleManagedReference";
        }
        return "Unknown";
    }
    bool bind(Il2CppClass* cls,const char* fieldName,Kind expected,bool stat,std::string* error,
              Il2CppClass* referenceClass=nullptr,bool forceGetter=false) {
        auto& a=api();label=name(cls)+"."+fieldName;owner=cls;kind=expected;isStatic=stat;
        std::string missing;
        const auto require=[&](auto function,const char* symbol){if(!function){if(!missing.empty())missing+=",";missing+=symbol;}};
        require(a.class_get_name,"class_get_name");require(a.class_get_namespace,"class_get_namespace");
        require(a.class_get_parent,"class_get_parent");require(a.class_get_field_from_name,"class_get_field_from_name");
        require(a.class_from_il2cpp_type,"class_from_il2cpp_type");require(a.class_is_valuetype,"class_is_valuetype");
        require(a.class_value_size,"class_value_size");require(a.field_get_type,"field_get_type");
        require(a.field_get_flags,"field_get_flags");require(a.field_static_get_value,"field_static_get_value");
        require(a.method_get_param,"method_get_param");require(a.method_get_param_count,"method_get_param_count");
        require(a.method_get_return_type,"method_get_return_type");require(a.method_get_flags,"method_get_flags");
        require(a.class_get_method_from_name,"class_get_method_from_name");require(a.type_get_name,"type_get_name");
        require(a.il2cpp_free,"il2cpp_free");require(a.object_get_class,"object_get_class");
        require(a.class_is_assignable_from,"class_is_assignable_from");require(a.runtime_invoke,"runtime_invoke");
        require(a.class_get_rank,"class_get_rank");require(a.class_get_element_class,"class_get_element_class");
        if(!cls || !missing.empty())
            return reject(error,"Bind","CompleteMetadataAndOwner",cls?"MissingExports="+missing:"MissingOwnerClass","MetadataUnavailable");
        field=forceGetter?nullptr:findField(cls,fieldName);getter=nullptr;
        const Il2CppType* type=nullptr;
        if(field) {
            if(bool(a.field_get_flags(field)&0x10u)!=stat)return reject(error,"Bind",stat?"Static":"Instance","WrongStorage","NativeStorageMismatch");
            type=a.field_get_type(field);
        } else {
            const std::string method="get_"+std::string(fieldName);
            getter=a.class_get_method_from_name(cls,method.c_str(),0);
            if(!getter || a.method_get_param_count(getter)!=0 || bool(a.method_get_flags(getter,nullptr)&0x10u)!=stat ||
               (a.method_is_generic && a.method_is_generic(getter)))
                return reject(error,"Bind","FieldOrZeroArgumentGetter",getter?"WrongStorageOrSignature":"Missing","PropertyContractMismatch");
            type=a.method_get_return_type(getter);
        }
        char* nativeName=type?a.type_get_name(type):nullptr;
        const bool indirect=nativeName && (std::strchr(nativeName,'&') || std::strchr(nativeName,'*'));
        if(nativeName)a.il2cpp_free(nativeName);
        if(indirect)return reject(error,"Bind","ValueOrManagedReference","ByRefOrPointer","NativeIndirectionMismatch");
        declared=type?a.class_from_il2cpp_type(type):nullptr;
        if(!declared)return reject(error,"Bind","DeclaredType","Missing","NativeTypeMismatch");
        const bool value=a.class_is_valuetype(declared);uint32_t align=0;
        const int nativeSize=value?a.class_value_size(declared,&align):int(sizeof(void*));
        size=static_cast<size_t>(nativeSize>0?nativeSize:0);
        const std::string actual=name(declared);
        bool valid=false;
        if(expected==Kind::Boolean)valid=value && actual=="System.Boolean" && size==1;
        else if(expected==Kind::Int32)valid=value && actual=="System.Int32" && size==4;
        else if(expected==Kind::Single)valid=value && actual=="System.Single" && size==4;
        else if(expected==Kind::Double)valid=value && actual=="System.Double" && size==8;
        else if(expected==Kind::Vector2)valid=value && actual=="Microsoft.Xna.Framework.Vector2" && size==8;
        else if(expected==Kind::Reference)valid=!value && referenceClass && a.class_is_assignable_from(referenceClass,declared);
        else {
            auto* element=a.class_get_element_class(declared);
            const std::string elementName=name(element);
            const bool elementValue=element && a.class_is_valuetype(element);
            uint32_t elementAlign=0;
            const int elementSize=elementValue?a.class_value_size(element,&elementAlign):int(sizeof(void*));
            const bool primitive=expected==Kind::IntArray || expected==Kind::SingleArray || expected==Kind::BooleanArray;
            const char* primitiveName=expected==Kind::BooleanArray?"System.Boolean":expected==Kind::SingleArray?"System.Single":"System.Int32";
            const int primitiveSize=expected==Kind::BooleanArray?1:4;
            valid=!value && a.class_get_rank(declared)==1 && element && (
                primitive ? elementValue && elementName==primitiveName && elementSize==primitiveSize :
                !elementValue && referenceClass && element==referenceClass);
        }
        if(!valid)return reject(error,"Bind",expectedName(expected),actual+":"+std::to_string(size),"NativeTypeOrSizeMismatch");
        if(field && !stat) {
            if(!a.field_get_offset || !a.class_instance_size)return reject(error,"Bind","InstanceLayoutExports","Missing","MetadataUnavailable");
            offset=a.field_get_offset(field);const size_t total=a.class_instance_size(cls);
            if(offset<sizeof(Il2CppObject) || size>total || offset>total-size)
                return reject(error,"Bind","FieldWithinObject",std::to_string(offset)+"/"+std::to_string(total),"NativeOffsetOutOfBounds");
        }
        return true;
    }
    bool read(void* self,void* output,std::string* error) const {
        auto& a=api();
        if(!output || (!isStatic && (!self || !a.class_is_assignable_from(owner,a.object_get_class(static_cast<Il2CppObject*>(self))))))
            return reject(error,"Read","OutputAndCompatibleOwner","NullOrWrongOwner","NativeObjectMismatch");
        if(field) {
            if(isStatic)a.field_static_get_value(field,output);
            else std::memcpy(output,static_cast<const char*>(self)+offset,size);
            return true;
        }
        Il2CppObject* exception=nullptr;
        auto* result=a.runtime_invoke(getter,isStatic?nullptr:self,nullptr,&exception);
        if(exception) {
            char detail[1024]{};if(a.format_exception)a.format_exception(exception,detail,sizeof(detail));detail[sizeof(detail)-1]=0;
            return reject(error,"Getter","NoException",detail[0]?detail:name(a.object_get_class(exception)),"NativeGetterException");
        }
        if(a.class_is_valuetype(declared)) {
            if(!result || a.object_get_class(result)!=declared)return reject(error,"Getter",name(declared),result?name(a.object_get_class(result)):"Null","NativeBoxedTypeMismatch");
            std::memcpy(output,reinterpret_cast<const char*>(result)+sizeof(Il2CppObject),size);
        } else {
            if(result && !a.class_is_assignable_from(declared,a.object_get_class(result)))
                return reject(error,"Getter",name(declared),name(a.object_get_class(result)),"NativeReferenceTypeMismatch");
            std::memcpy(output,&result,sizeof(result));
        }
        return true;
    }
    bool array(Il2CppArray* value,size_t minimum,std::string* error) const {
        if(!value || api().object_get_class(reinterpret_cast<Il2CppObject*>(value))!=declared || value->bounds || value->length<minimum || value->length>16384)
            return reject(error,"ReadArray",name(declared)+":VectorLength:"+std::to_string(minimum)+"..16384",
                value?name(api().object_get_class(reinterpret_cast<Il2CppObject*>(value)))+":Length="+std::to_string(value->length)+":Bounds="+(value->bounds?"Present":"None"):"Null","NativeArrayContractMismatch");
        return true;
    }
};
}
