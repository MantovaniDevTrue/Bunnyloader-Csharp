#include "experiments/csharp/SummonPolicy.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
bl::il2cpp::Api contractApi;
std::map<std::string,FieldInfo> members;
namespace bl::il2cpp {Api& api(){return contractApi;}FieldInfo* findField(Il2CppClass*,std::string_view name){auto it=members.find(std::string(name));return it==members.end()?nullptr:&it->second;}}
int main(int argc,char** argv) {
 using namespace bl::csharp;
 Il2CppClass boolean{"System","Boolean",1,true},integer{"System","Int32",4,true},vector{"Microsoft.Xna.Framework","Vector2",8,true},array{"System","Int32[]",8,false},owner{"Terraria","Player",128,false};
 contractApi.class_from_il2cpp_type=[](const Il2CppType* type){return type->cls;};contractApi.field_get_type=[](FieldInfo* field)->const Il2CppType*{return &field->type;};
 contractApi.field_get_flags=[](FieldInfo* field){return field->flags;};contractApi.field_get_offset=[](FieldInfo* field){return field->offset;};
 contractApi.class_get_name=[](Il2CppClass* cls){return cls->name;};contractApi.class_get_namespace=[](Il2CppClass* cls){return cls->ns;};
 contractApi.class_is_valuetype=[](const Il2CppClass* cls){return cls->value;};contractApi.class_instance_size=[](Il2CppClass* cls){return cls->size;};
 contractApi.class_value_size=[](Il2CppClass* cls,uint32_t* alignment){*alignment=4;return static_cast<int32_t>(cls->size);};
 members={{"active",{{&boolean},16}},{"dead",{{&boolean},17}},{"buffType",{{&array},24}},{"buffTime",{{&array},32}},
          {"position",{{&vector},40}},{"width",{{&integer},48}},{"height",{{&integer},52}},{"direction",{{&integer},56}},
          {"MinionAttackTargetNPC",{{&integer},60}}};
 SummonOwnerFields fields;std::string error;assert(bindSummonOwnerFields(&owner,&fields,&error));assert(fields.manualTarget==60);
 const auto saved=fields;
 members["dead"].type.cls=&integer;assert(!bindSummonOwnerFields(&owner,&fields,&error)&&error.find("System.Int32")!=std::string::npos);assert(fields.manualTarget==saved.manualTarget);members["dead"].type.cls=&boolean;
 members["position"].offset=127;assert(!bindSummonOwnerFields(&owner,&fields,&error)&&error.find("InvalidFieldBounds")!=std::string::npos);members["position"].offset=40;
 vector.size=16;assert(!bindSummonOwnerFields(&owner,&fields,&error)&&error.find("NativeSizeMismatch")!=std::string::npos);vector.size=8;
 members["active"].flags=0x10;assert(!bindSummonOwnerFields(&owner,&fields,&error));members["active"].flags=0;
 members.erase("buffTime");assert(!bindSummonOwnerFields(&owner,&fields,&error)&&error.find("Missing")!=std::string::npos);
 SummonMovement policy;assert(argc==2);std::ifstream file(argv[1]);std::string row,payload;
 while(std::getline(file,row))if(row.rfind("PROJECTILE\tExampleMinion\t",0)==0)payload=row.substr(row.find_last_of('\t')+1);
 assert(!payload.empty()&&parseSummonMovement(payload,&policy,&error));assert(policy.enabled&&policy.followSpeed==4&&policy.catchInertia==60&&policy.ownerLeash==1200);
 const auto valid=policy;
 for(const auto& bad:{payload+",",payload+",9",std::string("NaN,")+payload.substr(2),std::string("41,")+payload.substr(2)}) {
   assert(!parseSummonMovement(bad,&policy,&error));assert(policy.ownerLeash==valid.ownerLeash&&policy.followSpeed==valid.followSpeed);
 }
 assert(summonSourceDamage(90,30,45)==90&&summonSourceDamage(0,30,45)==30&&summonSourceDamage(0,-1,45)==45&&summonSourceDamage(0,0,45)==0&&summonSourceDamage(0,0,0)==0);
 std::cout<<"SUMMON_TYPED_OWNER_BINDINGS_TRANSACTIONAL_REJECTION_REAL_COMPILED_MOVEMENT_DESCRIPTOR_PASS\n";
}
