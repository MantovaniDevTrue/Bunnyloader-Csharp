#include "experiments/csharp/ManagedTileCollisionContract.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
struct Class;
struct Type {const char* name;Class* klass;};
struct Field {const char* name;Type* type;uint32_t flags;size_t offset;};
struct Class {const char* ns;const char* name;int size;bool value;std::vector<Field*> fields;};
struct Method {Class* owner;bool instance,generic;Type* result;std::vector<Type*> parameters;};
template<class T,class U>T* as(U* p){return reinterpret_cast<T*>(p);}
}
namespace bl::il2cpp {
Api& api(){static Api a;return a;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(cls)for(auto* f:as<Class>(cls)->fields)if(name==f->name)return as<FieldInfo>(f);
    return nullptr;
}
}

int main() {
    using namespace bl::csharp;
    auto& a=bl::il2cpp::api();
    a.type_get_name=[](const Il2CppType* t){return strdup(as<const Type>(t)->name);};
    a.il2cpp_free=std::free;
    a.class_from_il2cpp_type=[](const Il2CppType* t){return as<Il2CppClass>(as<const Type>(t)->klass);};
    a.class_get_name=[](Il2CppClass* c){return as<Class>(c)->name;};
    a.class_get_namespace=[](Il2CppClass* c){return as<Class>(c)->ns;};
    a.class_is_valuetype=[](const Il2CppClass* c){return as<const Class>(c)->value;};
    a.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=4;return as<Class>(c)->size;};
    a.class_get_fields=[](Il2CppClass* c,void** iterator)->FieldInfo* {
        auto& fields=as<Class>(c)->fields;const auto index=reinterpret_cast<size_t>(*iterator);
        if(index>=fields.size())return nullptr;
        *iterator=reinterpret_cast<void*>(index+1);return as<FieldInfo>(fields[index]);
    };
    a.field_get_type=[](FieldInfo* f){return as<const Il2CppType>(as<Field>(f)->type);};
    a.field_get_name=[](FieldInfo* f){return as<Field>(f)->name;};
    a.field_get_flags=[](FieldInfo* f){return as<Field>(f)->flags;};
    a.field_get_offset=[](FieldInfo* f){return as<Field>(f)->offset;};
    a.field_static_get_value=[](FieldInfo*,void*){assert(false&&"binding must not read static data");};
    a.method_is_instance=[](const MethodInfo* m){return as<const Method>(m)->instance;};
    a.method_is_generic=[](const MethodInfo* m){return as<const Method>(m)->generic;};
    a.method_get_class=[](const MethodInfo* m){return as<Il2CppClass>(as<const Method>(m)->owner);};
    a.method_get_return_type=[](const MethodInfo* m){return as<const Il2CppType>(as<const Method>(m)->result);};
    a.method_get_param_count=[](const MethodInfo* m){return static_cast<uint32_t>(as<const Method>(m)->parameters.size());};
    a.method_get_param=[](const MethodInfo* m,uint32_t i){return as<const Il2CppType>(as<const Method>(m)->parameters.at(i));};

    Class real{"System","Single",4,true,{}},integer{"System","Int32",4,true,{}},
          vector{"Microsoft.Xna.Framework","Vector2",8,true,{}},
          projectile{"Terraria","Projectile",4096,false,{}},main{"Terraria","Main",4096,false,{}};
    Type single{"System.Single",&real},int32{"System.Int32",&integer},
         vec{"Microsoft.Xna.Framework.Vector2",&vector},vecRef{"Microsoft.Xna.Framework.Vector2&",&vector},
         intRef{"System.Int32&",&integer},voidType{"System.Void",nullptr};
    Field x{"X",&single,0,0},y{"Y",&single,0,4},staticVector{"zeroVector",&vec,0x10,0};
    vector.fields={&staticVector,&x,&y};
    Field netMode{"netMode",&int32,0x10,0};main.fields={&netMode};
    Method movement{&projectile,true,false,&voidType,{&vec}},kill{&projectile,true,false,&voidType,{}},
           params{&projectile,true,false,&voidType,{&vecRef,&intRef,&intRef}};
    std::string error;int rejected=0;
    auto valid=[&](bool box=true){return validateMobileTileCollisionMethods(as<Il2CppClass>(&projectile),
        as<MethodInfo>(&movement),as<MethodInfo>(&kill),box?as<MethodInfo>(&params):nullptr,box,&error);};
    auto reject=[&]{assert(!valid()&&!error.empty());++rejected;};
    assert(valid()&&error.empty());assert(valid(false));
    x.offset=sizeof(Il2CppObject);y.offset=sizeof(Il2CppObject)+4;assert(valid());
    x.offset=0;y.offset=4;
    movement.instance=false;reject();movement.instance=true;
    kill.instance=false;reject();kill.instance=true;
    params.instance=false;reject();params.instance=true;
    movement.generic=true;reject();movement.generic=false;
    params.result=&int32;reject();params.result=&voidType;
    movement.owner=&main;reject();movement.owner=&projectile;
    params.parameters[0]=&vec;reject();params.parameters[0]=&vecRef;
    params.parameters[1]=&int32;reject();params.parameters[1]=&intRef;
    params.parameters.pop_back();reject();params.parameters.push_back(&intRef);
    movement.parameters.push_back(&vec);reject();movement.parameters.pop_back();
    assert(!validateMobileTileCollisionMethods(as<Il2CppClass>(&projectile),as<MethodInfo>(&movement),
        as<MethodInfo>(&kill),nullptr,true,&error)&&!error.empty());++rejected;
    intRef.name="System.Int64&";reject();intRef.name="System.Int32&";
    vec.name="UnityEngine.Vector2";reject();vec.name="Microsoft.Xna.Framework.Vector2";
    vector.ns="UnityEngine";reject();vector.ns="Microsoft.Xna.Framework";
    vector.value=false;reject();vector.value=true;
    vector.size=16;reject();vector.size=8;
    y.offset=0;reject();y.offset=4;
    y.offset=8;reject();y.offset=4;
    y.name="Z";reject();y.name="Y";
    single.name="System.Int32";real.name="Int32";reject();single.name="System.Single";real.name="Single";
    Field alias{"Y",&single,0,4};vector.fields.push_back(&alias);reject();vector.fields.pop_back();
    auto savedGeneric=a.method_is_generic;a.method_is_generic=nullptr;reject();a.method_is_generic=savedGeneric;
    auto savedName=a.type_get_name;a.type_get_name=nullptr;reject();a.type_get_name=savedName;
    auto savedFields=a.class_get_fields;a.class_get_fields=nullptr;reject();a.class_get_fields=savedFields;
    auto savedClass=a.class_from_il2cpp_type;a.class_from_il2cpp_type=nullptr;reject();a.class_from_il2cpp_type=savedClass;
    assert(valid());
    FieldInfo* bound=nullptr;
    auto bindNet=[&]{return mobileTileCollisionNetMode(as<Il2CppClass>(&main),&bound,&error);};
    assert(bindNet()&&bound==as<FieldInfo>(&netMode));
    auto rejectNet=[&]{bound=nullptr;assert(!bindNet()&&!bound&&!error.empty());++rejected;};
    netMode.flags=0;rejectNet();netMode.flags=0x10;
    integer.size=8;rejectNet();integer.size=4;
    integer.name="UInt32";rejectNet();integer.name="Int32";
    integer.value=false;rejectNet();integer.value=true;
    main.fields.clear();rejectNet();main.fields={&netMode};
    auto savedRead=a.field_static_get_value;a.field_static_get_value=nullptr;rejectNet();a.field_static_get_value=savedRead;
    assert(bindNet());
    std::cout<<"MOBILE_COLLISION_METHOD_VECTOR_AND_NETMODE_CONTRACT_PASS rejected="<<rejected<<'\n';
}
