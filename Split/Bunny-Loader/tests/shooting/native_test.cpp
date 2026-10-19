#include "experiments/csharp/NativeShooting.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdarg>

struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;int rank=0;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {void* pointer=nullptr;bool instance=false;int id=0;};
void applyTime(Il2CppObject*,Il2CppObject*,const MethodInfo*);
bl::il2cpp::Api fixture;
Il2CppClass player{"Terraria","Player",128,false},item{"Terraria","Item",128,false},projectile{"Terraria","Projectile",128,false},mainClass{"Terraria","Main",128,false},
 integerClass{"System","Int32",4,true},reference{"Terraria.DataStructures","IEntitySource",16,false},modifierClass{"Terraria","NewProjectileModifier",16,false},
 arrayClass{"Terraria","Projectile[]",16,false,1,&projectile};
Il2CppType sourceType{&reference},modifierType{&modifierClass};
FieldInfo typeField{{&integerClass},16},whoField{{&integerClass},20},poolField{{&arrayClass},0,0x10};
MethodInfo useMethod{nullptr,true,1},spawnMethod{nullptr,false,2},timeMethod{reinterpret_cast<void*>(applyTime),true,3};
struct Pool {Il2CppArray array;Il2CppObject* data[1001]{};} pool{{{&arrayClass,nullptr},nullptr,1001},{}};
int useCalls=0,timeCalls=0,callbackCalls=0,warmState=1,warnings=0,spawnCountPerUse=1;bool callbackAvailable=true,failQueue=false,missingMethod=false,throwUse=false,throwSpawn=false;
std::string managedOutput,canOutput="1",lastSnapshot;
struct Spawn {Il2CppObject* source;float x,y,vx,vy;int type,damage;float kb;int owner;float a0,a1,a2;Il2CppObject* modifier;};
std::vector<Spawn> spawned;std::vector<int> factoryResults;
Il2CppObject source{&reference,nullptr},modifierObject{&modifierClass,nullptr};
void applyTime(Il2CppObject*,Il2CppObject*,const MethodInfo* m){assert(m==&timeMethod);++timeCalls;}
int originalProjectile(Il2CppObject* src,float x,float y,float vx,float vy,int type,int damage,float kb,int owner,float a0,float a1,float a2,Il2CppObject* modifier,const MethodInfo*) {
 if(throwSpawn)throw 42;
 assert(!bl::csharp::shooting::active.policy);spawned.push_back({src,x,y,vx,vy,type,damage,kb,owner,a0,a1,a2,modifier});return 17;
}
void originalItemCheck(Il2CppObject*,int,Il2CppObject*,int,bool,const MethodInfo*) {
 ++useCalls;if(throwUse)throw 42;
 for(int i=0;i<spawnCountPerUse;i++)factoryResults.push_back(bl::csharp::shooting::spawn(&source,100,200,10,0,1136,24,7,2,1,2,3,&modifierObject,&spawnMethod));
}
namespace bl::il2cpp {
bool guardedCall(void(*body)(void*),void* argument,Caught* caught){try{body(argument);return true;}catch(int){if(caught)caught->threw=true;return false;}}
std::string describeCaught(const Caught& caught){assert(caught.threw);return "original-native-test-error";}
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& type){return type.name=="Item"?&item:type.name=="Player"?&player:type.name=="Projectile"?&projectile:&mainClass;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls==&item&&name=="type"?&typeField:cls==&player&&name=="whoAmI"?&whoField:cls==&mainClass&&name=="projectile"?&poolField:nullptr;}
Signature parseSignature(std::string_view text){Signature signature;signature.name=text;return signature;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo**) {
 if(missingMethod)return nullptr;
 return sig.name.find("ItemCheck_Shoot")!=std::string::npos?&useMethod:sig.name.find("NewProjectile(")!=std::string::npos?&spawnMethod:&timeMethod;
}
}
namespace bl::hook {bool install(const MethodInfo* m,void*,void** original){*original=m==&useMethod?reinterpret_cast<void*>(originalItemCheck):reinterpret_cast<void*>(originalProjectile);return true;}}
namespace bl::runtime {int projectileTypeCount(){return 1140;}int modProjectileTypeByName(const std::string& mod,const std::string& name){return mod=="other"&&name=="Projectile"?1139:-1;}}
namespace bl::log {void write(int,const char*,...){++warnings;}}
namespace bl::csharp {
bool queueManagedShootWarmup(const std::string&,const std::string&,const std::string&){return !failQueue;}
int managedShootWarmupState(const std::string&,const std::string&,const std::string&,std::string* error){*error="ERROR|Reason=OriginalWarmupError";return warmState;}
bool invokeManagedShoot(const std::string&,const std::string&,const std::string&,int kind,const std::string& snapshot,std::string* output){++callbackCalls;lastSnapshot=snapshot;*output=kind==0?canOutput:managedOutput;return callbackAvailable;}
}
int main(int argc,char** argv) {
 using namespace bl::csharp::shooting;assert(argc==3);
 std::ifstream outputFile(argv[1]);std::getline(outputFile,managedOutput);
 std::ifstream wire(argv[2]);std::string row;bool found=false;
 while(std::getline(wire,row))if(row=="SHOOT\tExampleShotgun\t4")found=true;
 assert(found);
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return static_cast<int32_t>(c->size);};
 fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
 fixture.field_static_get_value=[](FieldInfo* f,void* out){assert(f==&poolField);*static_cast<Il2CppArray**>(out)=&pool.array;};
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};fixture.method_get_param=[](const MethodInfo* m,uint32_t index)->const Il2CppType*{assert(m==&spawnMethod);return index==0?&sourceType:&modifierType;};
 std::string error;Entry entry{"independent","ExampleShotgun","Actual/ExampleShotgun.cs","",6230,7};
 assert(publish({entry},&error)&&hasPolicies("independent"));assert(bindAssembly("independent","independent.dll",&error));
 alignas(8) char playerBytes[128]{},itemBytes[128]{};int type=6230,who=2;std::memcpy(itemBytes+16,&type,4);std::memcpy(playerBytes+20,&who,4);
 auto* p=reinterpret_cast<Il2CppObject*>(playerBytes);auto* i=reinterpret_cast<Il2CppObject*>(itemBytes);
 use(p,2,i,18,true,&useMethod);assert(useCalls==1&&callbackCalls==2&&spawned.size()==8&&!active.policy);
 assert(factoryResults.back()==1000); // The 1001-cell pool must not become the factory sentinel.
 for(const auto& shot:spawned)assert(shot.source==&source&&shot.modifier==&modifierObject&&shot.owner==2&&shot.type==1136&&shot.damage==24&&shot.kb==7&&shot.a0==1&&shot.a1==2&&shot.a2==3);
 assert(lastSnapshot.starts_with("2,6230,18,"));
 // Two native calls each receive their own callback. Own emitted projectiles do not reenter.
 spawned.clear();spawnCountPerUse=2;use(p,2,i,18,true,&useMethod);assert(spawned.size()==16&&callbackCalls==5);spawnCountPerUse=1;
 canOutput="0";spawned.clear();use(p,2,i,18,true,&useMethod);assert(spawned.empty()&&timeCalls==1);canOutput="1";
 auto output=managedOutput;managedOutput="SHOT1|0|v1136,100,200,10,0,24,7,1,2,3|";
 use(p,2,i,18,true,&useMethod);assert(spawned.empty());
 managedOutput="SHOT1|1|v1136,100,200,10,0,48,7,1,2,3|";
 use(p,2,i,18,true,&useMethod);assert(spawned.size()==1&&spawned[0].damage==48);spawned.clear();
 // Transactional validation: a valid first command must not escape if a later command is invalid.
 managedOutput="SHOT1|0|v1136,100,200,10,0,24,7,1,2,3|v1136,100,200,10,0,24,7,1,2,3;v999999,100,200,10,0,24,7,1,2,3";
 use(p,2,i,18,true,&useMethod);assert(spawned.empty());
 Result decoded;assert(!decode(managedOutput,entry,&decoded,&error));assert(error.find("InvalidShotOutput")!=std::string::npos);
 auto named="SHOT1|0|v1136,100,200,10,0,24,7,1,2,3|mother/Projectile,100,200,10,0,24,7,1,2,3";
 assert(decode(named,entry,&decoded,&error)&&decoded.additional[0].type==1139);
 assert(!decode(std::string(33000,'x'),entry,&decoded,&error));
 managedOutput="ERROR|Reason=OriginalManagedException";use(p,2,i,18,true,&useMethod);assert(spawned.empty());
 callbackAvailable=false;use(p,2,i,18,true,&useMethod);assert(spawned.empty()&&timeCalls==2);callbackAvailable=true;
 managedOutput=output;warmState=0;assert(bindAssembly("independent","pending.dll",&error));use(p,2,i,18,true,&useMethod);assert(spawned.empty()&&timeCalls==3);
 warmState=-1;use(p,2,i,18,true,&useMethod);assert(spawned.empty()&&timeCalls==4);warmState=1;
 assert(bindAssembly("independent","ready.dll",&error));type=1;std::memcpy(itemBytes+16,&type,4);use(p,2,i,18,true,&useMethod);assert(spawned.size()==1);spawned.clear();
 type=6230;std::memcpy(itemBytes+16,&type,4);
 throwUse=true;use(p,2,i,18,true,&useMethod);assert(!active.policy&&spawned.empty());throwUse=false;
 throwSpawn=true;use(p,2,i,18,true,&useMethod);assert(!active.policy&&spawned.empty());throwSpawn=false;
 use(p,2,i,18,true,&useMethod);assert(spawned.size()==8&&!active.policy);spawned.clear();
 pool.array.length=999;const int previousTime=timeCalls;
 use(p,2,i,18,true,&useMethod);assert(timeCalls==previousTime+1&&spawned.empty());pool.array.length=1001;
 auto table=std::atomic_load(&policies);entry.mask=8;assert(!publish({entry},&error)&&std::atomic_load(&policies)==table);entry.mask=7;
 failQueue=true;assert(!bindAssembly("independent","failed.dll",&error)&&std::atomic_load(&policies)==table);failQueue=false;
 // Actual binder rejects mismatched widths, storage, bounds, factory ABI and method staticness.
 originalUse=nullptr;originalSpawn=nullptr;whoField.flags=0x10;assert(!prepare(&error));whoField.flags=0;
 whoField.offset=128;assert(!prepare(&error));whoField.offset=20;
 integerClass.size=2;assert(!prepare(&error));integerClass.size=4;
 modifierClass.value=true;assert(!prepare(&error)&&error.find("NativeAbiMismatch")!=std::string::npos);modifierClass.value=false;
 spawnMethod.instance=true;assert(!prepare(&error));spawnMethod.instance=false;
 missingMethod=true;assert(!prepare(&error));missingMethod=false;
 assert(prepare(&error));
 std::cout<<"SHOOT_PRODUCTION_HOOKS_TRANSACTION_SOURCE_OWNER_AMMO_DAMAGE_AI_CANSHOOT_REENTRY_WARMUP_FAILURE_CONTRACTS_AND_COMPILED_SHOTGUN_PASS\n";
}
