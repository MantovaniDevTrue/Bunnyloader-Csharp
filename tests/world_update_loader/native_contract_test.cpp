#include "il2cpp/Api.h"
#include <string>
#include <vector>
struct Il2CppClass { const char* name; int size; bool value=true; const char* ns="System"; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType type; uint32_t flags=0x10; };
struct MethodInfo { uint32_t flags=0x10; };
#include "experiments/csharp/SystemStageContracts.h"
#include <cassert>
#include <iostream>
namespace {
bl::il2cpp::Api fixture;
Il2CppClass single{"Single",4}, doubleType{"Double",8}, boolean{"Boolean",1}, integer{"Int32",4}, incompatible{"Int64",8};
Il2CppClass owner{"Main",0,false,"Terraria"};
FieldInfo field{"dayRate",{&single}};
}
namespace bl::il2cpp {
Api& api(){return fixture;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls==&owner && name==field.name ? &field : nullptr;}
}
int main(){
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};
 fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->size);};
 fixture.field_static_get_value=[](FieldInfo*,void*){assert(false && "Contract resolution must not read a value");};
 fixture.field_static_set_value=[](FieldInfo*,void*){assert(false && "Contract resolution must not write a value");};
 fixture.method_get_flags=[](const MethodInfo* m,uint32_t*){return m->flags;};
 std::string error;bl::csharp::SystemStageFieldKind kind{};
 assert(bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error)==&field && kind==bl::csharp::SystemStageFieldKind::Single);
 field.type.cls=&doubleType;
 assert(bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error)==&field && kind==bl::csharp::SystemStageFieldKind::Double);
 field.type.cls=&integer;
 assert(bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error)==&field && kind==bl::csharp::SystemStageFieldKind::Int32);
 field.type.cls=&incompatible;
 assert(!bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error));
 assert(error.find("Actual=System.Int64:8")!=error.npos && error.find("API=BunnyWorldUpdateLoader")!=error.npos);
 field.type.cls=&doubleType; doubleType.size=4;
 assert(!bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error));
 assert(error.find("Actual=System.Double:4")!=error.npos);
 doubleType.size=8;
 for(auto invalid:{0u,0x30u,0x50u}){
  field.flags=invalid;
  assert(!bl::csharp::systemStageField(&owner,"dayRate",true,&kind,&error));
  assert(error.find("NativeStorageMismatch")!=error.npos);
 }
 field.flags=0x10;field.name="gameMenu";field.type.cls=&boolean;
 assert(bl::csharp::systemStageField(&owner,"gameMenu",false,nullptr,&error)==&field);
 boolean.size=4;
 assert(!bl::csharp::systemStageField(&owner,"gameMenu",false,nullptr,&error));
 boolean.size=1;
 assert(!bl::csharp::systemStageField(&owner,"absent",false,nullptr,&error));
 assert(error.find("Field=absent")!=error.npos && error.find("RequiredFieldMissing")!=error.npos);
 fixture.field_get_flags=nullptr;
 assert(!bl::csharp::systemStageField(&owner,"gameMenu",false,nullptr,&error));
 MethodInfo method;
 assert(bl::csharp::systemStageMethodStorage(&method,true,"Main.UpdateTimeRate",&error));
 assert(!bl::csharp::systemStageMethodStorage(&method,false,"Main.DoUpdateInWorld",&error));
 assert(error.find("Expected=Instance Actual=Static")!=error.npos);
 method.flags=0;
 assert(bl::csharp::systemStageMethodStorage(&method,false,"Main.DoUpdateInWorld",&error));
 assert(!bl::csharp::systemStageMethodStorage(nullptr,true,"Main.UpdateTimeRate",&error));
 std::cout<<"WORLD_UPDATE_NATIVE_FIELD_STORAGE_SIZE_AND_METHOD_ABI_CONTRACTS=PASS\n";
}
