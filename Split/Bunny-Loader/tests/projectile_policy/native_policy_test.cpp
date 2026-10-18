#include "experiments/csharp/ProjectilePolicy.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
struct Il2CppClass {std::string ns,name;size_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {std::string name;Il2CppType type;size_t offset;uint32_t flags;};
struct Projectile {Il2CppObject obj;bool minion=true;float slots=1;int frame=0,counter=0;uint32_t sentinel=0xabcdef12;};
bl::il2cpp::Api instance;
Il2CppClass projectile{"Terraria","Projectile",sizeof(Projectile),false},boolean{"System","Boolean",1,true},single{"System","Single",4,true},integer{"System","Int32",4,true},intArray{"System","Int32[]",0,false},mainClass{"Terraria","Main",sizeof(Il2CppObject),false};
Il2CppArray* frameTable=nullptr;
std::vector<FieldInfo> fields;
namespace bl::il2cpp {
Api& api(){return instance;}
FieldInfo* findField(Il2CppClass*,std::string_view name){for(auto& f:fields)if(f.name==name)return &f;return nullptr;}
}
int main(int argc,char** argv){
 using namespace bl::csharp;
 instance.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 instance.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 instance.field_get_flags=[](FieldInfo* f){return f->flags;};
 instance.field_get_offset=[](FieldInfo* f){return f->offset;};
 instance.class_get_name=[](Il2CppClass* c){return c->name.c_str();};
 instance.class_get_namespace=[](Il2CppClass* c){return c->ns.c_str();};
 instance.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 instance.class_instance_size=[](Il2CppClass* c){return static_cast<uint32_t>(c->size);};
 instance.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=4;return static_cast<int32_t>(c->size);};
 instance.class_get_rank=[](const Il2CppClass* c){return c==&intArray?1:0;};
 instance.class_get_element_class=[](Il2CppClass* c)->Il2CppClass*{return c==&intArray?&integer:nullptr;};
 instance.field_static_get_value=[](FieldInfo*,void* destination){std::memcpy(destination,&frameTable,sizeof frameTable);};
 fields={{"minion",{&boolean},offsetof(Projectile,minion),0},{"minionSlots",{&single},offsetof(Projectile,slots),0},
         {"frame",{&integer},offsetof(Projectile,frame),0},{"frameCounter",{&integer},offsetof(Projectile,counter),0}};
 ProjectileGameplayFields bound;std::string error;
 assert(bindProjectileGameplayFields(&projectile,true,true,&bound,&error));
 Projectile object{{&projectile,nullptr}};ProjectilePolicy policy{4,5,3,900,true};
 assert(validateProjectilePolicy(policy,&error));
 for(int tick=0;tick<4;++tick)assert(advanceProjectileFrame(&object.obj,bound,policy));
 assert(object.frame==0&&object.counter==4);advanceProjectileFrame(&object.obj,bound,policy);assert(object.frame==1&&object.counter==0);
 for(int tick=0;tick<15;++tick)advanceProjectileFrame(&object.obj,bound,policy);
 assert(object.frame==0&&object.counter==0&&object.sentinel==0xabcdef12);
 object.frame=INT32_MAX;object.counter=INT32_MAX;advanceProjectileFrame(&object.obj,bound,policy);assert(object.frame==0&&object.counter==0);
 // Actual static table contract, including element size, rank and bounds.
 Il2CppArray nativeTable{{&intArray,nullptr},nullptr,1200};frameTable=&nativeTable;
 fields.push_back({"projFrames",{&intArray},0,0x10});
 assert(bindProjectileFrameTable(&mainClass,1136,&error));
 assert(!bindProjectileFrameTable(&mainClass,1200,&error)&&error.find("NativeArrayMismatch")!=std::string::npos);
 integer.size=8;assert(!bindProjectileFrameTable(&mainClass,1136,&error));integer.size=4;
 frameTable=nullptr;assert(!bindProjectileFrameTable(&mainClass,1136,&error));
 fields.pop_back();
 const auto saved=bound;
 fields[0].type.cls=&integer;
 assert(!bindProjectileGameplayFields(&projectile,true,true,&bound,&error)&&error.find("Expected=Instance:System.Boolean Actual=System.Int32")!=std::string::npos);
 assert(bound.frame==saved.frame&&object.sentinel==0xabcdef12);fields[0].type.cls=&boolean;
 boolean.size=4;assert(!bindProjectileGameplayFields(&projectile,true,true,&bound,&error)&&error.find("NativeSizeMismatch")!=std::string::npos);boolean.size=1;
 fields[3].offset=sizeof(Projectile);assert(!bindProjectileGameplayFields(&projectile,true,true,&bound,&error)&&error.find("InvalidFieldBounds")!=std::string::npos);fields[3].offset=offsetof(Projectile,counter);
 fields[2].flags=0x10;assert(!bindProjectileGameplayFields(&projectile,true,true,&bound,&error));fields[2].flags=0x20;
 assert(!bindProjectileGameplayFields(&projectile,true,true,&bound,&error)&&error.find("InitOnly")!=std::string::npos);fields[2].flags=0;
 fields.pop_back();assert(!bindProjectileGameplayFields(&projectile,false,true,&bound,&error)&&error.find("Actual=Missing")!=std::string::npos);
 assert(bindProjectileGameplayFields(&projectile,false,false,&bound,&error));assert(!advanceProjectileFrame(&object.obj,bound,policy));
 for(int invalid: {0,121}){auto bad=policy;bad.animationFrameTicks=invalid;assert(!validateProjectilePolicy(bad,&error));bad=policy;bad.minionScanTicks=invalid;assert(!validateProjectilePolicy(bad,&error));}
 for(float invalid: {0.f,4001.f,INFINITY,NAN}){auto bad=policy;bad.minionTargetRange=invalid;assert(!validateProjectilePolicy(bad,&error));}
 if(argc>1){
  assert(argc==2);std::ifstream file(argv[1]);std::string row;
  assert(std::getline(file,row)&&row=="BUNNY_MANAGED_DEFAULTS_V2");assert(std::getline(file,row));
  std::istringstream stream(row);std::vector<std::string> parts;std::string part;
  while(std::getline(stream,part,'\t'))parts.push_back(part);
  ProjectilePolicy decoded;decoded.frames=4;assert(parseProjectileTuning(parts,&decoded,&error));
  assert(decoded.animationFrameTicks==5&&decoded.minionScanTicks==3&&decoded.minionTargetRange==950.f);
  parts[15]="950junk";assert(!parseProjectileTuning(parts,&decoded,&error)&&error.find("InvalidNumericValue")!=std::string::npos);
  parts[15]="NaN";assert(!parseProjectileTuning(parts,&decoded,&error)&&error.find("ProjectilePolicyOutOfRange")!=std::string::npos);
  assert(decoded.minionTargetRange==950.f); // A rejected parse never partially changes installed settings.
 }
 std::cout<<"PROJECTILE_POLICY_TYPED_BOUNDED_WRITABLE_BINDING_ANIMATION_SENTINELS_AND_LIMITS_PASS\n";
}
