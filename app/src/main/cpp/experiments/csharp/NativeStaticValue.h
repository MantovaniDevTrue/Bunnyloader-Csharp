#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cstring>
#include <string>

namespace bl::csharp::native_static {
using namespace bl::il2cpp;
struct Value {
    FieldInfo* field=nullptr;
    const MethodInfo* getter=nullptr;
    const MethodInfo* setter=nullptr;
    Il2CppClass* type=nullptr;
    explicit operator bool() const {return type&&(field||getter);}
    bool writable() const {return field?!(api().field_get_flags(field)&0x40u):setter!=nullptr;}
};
inline Value resolve(Il2CppClass* cls,const char* name) {
    auto& a=api();Value value;if(!cls)return value;
    if(a.runtime_class_init)a.runtime_class_init(cls);
    if(auto* f=findField(cls,name)) {
        // Android Main exposes some statics through accessors while an inherited
        // instance field has the same name. That field is not the static value;
        // continue to the explicitly static getter instead of rejecting it.
        if(a.field_get_flags(f)&0x10u) {
            value.field=f;value.type=a.class_from_il2cpp_type(a.field_get_type(f));return value;
        }
    }
    if(!a.class_get_method_from_name||!a.method_get_return_type||!a.method_get_param_count||!a.method_is_instance)return value;
    const auto* getter=a.class_get_method_from_name(cls,(std::string("get_")+name).c_str(),0);
    if(!getter||a.method_is_instance(getter)||a.method_get_param_count(getter)!=0)return value;
    value.type=a.class_from_il2cpp_type(a.method_get_return_type(getter));value.getter=getter;
    const auto* setter=a.class_get_method_from_name(cls,(std::string("set_")+name).c_str(),1);
    if(setter&&!a.method_is_instance(setter)&&a.method_get_param_count(setter)==1&&a.method_get_param&&
       a.class_from_il2cpp_type(a.method_get_param(setter,0))==value.type) {
        auto* ret=a.class_from_il2cpp_type(a.method_get_return_type(setter));
        const char* ns=ret?a.class_get_namespace(ret):nullptr,*type=ret?a.class_get_name(ret):nullptr;
        if(ns&&type&&!std::strcmp(ns,"System")&&!std::strcmp(type,"Void"))value.setter=setter;
    }
    return value;
}
inline size_t size(const Value& value) {
    if(!value)return 0;
    uint32_t align=0;const int n=api().class_is_valuetype(value.type)?api().class_value_size(value.type,&align):static_cast<int>(sizeof(void*));
    return n>0&&n<=65536?static_cast<size_t>(n):0;
}
inline bool read(const Value& value,void* out,size_t bytes) {
    auto& a=api();if(!out||size(value)!=bytes)return false;
    if(value.field){if(!a.field_static_get_value)return false;a.field_static_get_value(value.field,out);return true;}
    if(!a.runtime_invoke||!a.object_get_class)return false;
    Il2CppObject* exception=nullptr;auto* result=a.runtime_invoke(value.getter,nullptr,nullptr,&exception);
    if(exception)return false;
    if(!a.class_is_valuetype(value.type)){std::memcpy(out,&result,sizeof(result));return true;}
    if(!result||a.object_get_class(result)!=value.type)return false;
    std::memcpy(out,reinterpret_cast<const char*>(result)+sizeof(Il2CppObject),bytes);return true;
}
inline bool write(const Value& value,void* data,size_t bytes) {
    if(!value.writable()||size(value)!=bytes)return false;
    void* argument=data;
    if(!api().class_is_valuetype(value.type))std::memcpy(&argument,data,sizeof(argument));
    if(value.field){if(!api().field_static_set_value)return false;api().field_static_set_value(value.field,argument);return true;}
    if(!api().runtime_invoke)return false;
    void* args[]={argument};Il2CppObject* exception=nullptr;
    api().runtime_invoke(value.setter,nullptr,args,&exception);return !exception;
}
inline bool integerKind(const Value& value,int* bytes,bool* sign) {
    auto& a=api();if(!value||!a.class_is_valuetype(value.type))return false;
    const char* ns=a.class_get_namespace(value.type),*name=a.class_get_name(value.type);
    if(!ns||!name||std::strcmp(ns,"System"))return false;
    struct Kind{const char* name;int bytes;bool sign;};
    for(const auto& k:{Kind{"SByte",1,true},{"Byte",1,false},{"Int16",2,true},{"UInt16",2,false},{"Int32",4,true},{"UInt32",4,false}})
        if(!std::strcmp(name,k.name)&&size(value)==static_cast<size_t>(k.bytes)){*bytes=k.bytes;*sign=k.sign;return true;}
    return false;
}
inline bool readInteger(const Value& value,int64_t* out) {
    if(!out)return false;
    int bytes=0;bool sign=false;uint64_t raw=0;
    if(!integerKind(value,&bytes,&sign)||!read(value,&raw,static_cast<size_t>(bytes)))return false;
    *out=sign?(bytes==1?static_cast<int8_t>(raw):bytes==2?static_cast<int16_t>(raw):static_cast<int32_t>(raw)):static_cast<int64_t>(raw);
    return true;
}
inline bool fitsInteger(const Value& value,int64_t number) {
    int bytes=0;bool sign=false;if(!integerKind(value,&bytes,&sign))return false;
    const int bits=bytes*8;const int64_t max=sign?(int64_t{1}<<(bits-1))-1:(int64_t{1}<<bits)-1;
    const int64_t min=sign?-(int64_t{1}<<(bits-1)):0;return number>=min&&number<=max;
}
inline bool writeInteger(const Value& value,int64_t number) {
    if(!fitsInteger(value,number))return false;
    uint64_t raw=static_cast<uint64_t>(number);return write(value,&raw,size(value));
}
} // namespace bl::csharp::native_static
