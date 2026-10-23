#include "experiments/csharp/NativeShooting.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdarg>
#include <limits>

struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;int rank=0;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {void* pointer=nullptr;bool instance=false;int id=0;};
void applyTime(Il2CppObject*,Il2CppObject*,const MethodInfo*);
bl::il2cpp::Api fixture;
Il2CppClass player{"Terraria","Player",128,false},item{"Terraria","Item",128,false},projectile{"Terraria","Projectile",128,false},mainClass{"Terraria","Main",128,false},
 vectorClass{"Microsoft.Xna.Framework","Vector2",8,true},floatClass{"System","Single",4,true},integerClass{"System","Int32",4,true},reference{"Terraria.DataStructures","IEntitySource",16,false},modifierClass{"Terraria","NewProjectileModifier",16,false},
 arrayClass{"Terraria","Projectile[]",16,false,1,&projectile};
Il2CppType sourceType{&reference},modifierType{&modifierClass};
FieldInfo typeField{{&integerClass},16},whoField{{&integerClass},20},poolField{{&arrayClass},0,0x10};
FieldInfo positionField{{&vectorClass},32},widthField{{&integerClass},40},directionField{{&integerClass},44},
 mouseField{{&vectorClass},0,0x10},localField{{&integerClass},0,0x10},xField{{&floatClass},0},yField{{&floatClass},4};
int localIndex=2,expectedDirection=0,aimReads=0;bool mouseProperty=false,mouseException=false,localProperty=true,localMissing=false,localException=false;
float mouseWorld[2]{200,100};
Il2CppType vectorType{&vectorClass},localType{&integerClass};
MethodInfo localGetter{nullptr,false,5};
struct BoxedInt {Il2CppObject object;int value;} localBox{{&integerClass,nullptr},0};
MethodInfo mouseGetter{nullptr,false,4},limitMethod{nullptr,true,6};
bool limitFailure=false;int limitCalls=0;
struct BoxedVector {Il2CppObject object;float xy[2];} mouseBox{{&vectorClass,nullptr},{}};
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
void originalItemCheck(Il2CppObject* p,int,Il2CppObject*,int,bool,const MethodInfo*) {
 if(expectedDirection){int facing=0;std::memcpy(&facing,reinterpret_cast<char*>(p)+44,4);assert(facing==expectedDirection);}
 ++useCalls;if(throwUse)throw 42;
 for(int i=0;i<spawnCountPerUse;i++)factoryResults.push_back(bl::csharp::shooting::spawn(&source,100,200,10,0,1136,24,7,2,1,2,3,&modifierObject,&spawnMethod));
}
namespace bl::il2cpp {
bool guardedCall(void(*body)(void*),void* argument,Caught* caught){try{body(argument);return true;}catch(int){if(caught)caught->threw=true;return false;}}
std::string describeCaught(const Caught& caught){assert(caught.threw);return "original-native-test-error";}
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& type){return type.name=="Item"?&item:type.name=="Player"?&player:type.name=="Projectile"?&projectile:&mainClass;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls==&item&&name=="type"?&typeField:cls==&player&&name=="whoAmI"?&whoField:cls==&mainClass&&name=="projectile"?&poolField:
 cls==&player&&name=="position"?&positionField:cls==&player&&name=="width"?&widthField:cls==&player&&name=="direction"?&directionField:
 cls==&mainClass&&name=="MouseWorld"&&!mouseProperty?&mouseField:cls==&mainClass&&name=="myPlayer"&&!localProperty?&localField:
 cls==&vectorClass&&name=="X"?&xField:cls==&vectorClass&&name=="Y"?&yField:nullptr;}
Signature parseSignature(std::string_view text){Signature signature;signature.name=text;return signature;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo**) {
 if(missingMethod)return nullptr;
 if(sig.name.find("LimitPointToPlayerReachableArea")!=std::string::npos)return &limitMethod;
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
 wire.clear();wire.seekg(0);int nativeOnly=0;while(std::getline(wire,row))if(row=="SHOOT\tExampleGun\t0"||row=="SHOOT\tExampleRocketLauncher\t0")++nativeOnly;assert(nativeOnly==2);
 wire.clear();wire.seekg(0);bool cursorDescriptor=false;
 while(std::getline(wire,row))if(row=="SHOOT\tExampleMinionItem\t0\t1")cursorDescriptor=true;
 assert(cursorDescriptor);
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return static_cast<int32_t>(c->size);};
 fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
 fixture.field_static_get_value=[](FieldInfo* f,void* out){if(f==&poolField)*static_cast<Il2CppArray**>(out)=&pool.array;
 else if(f==&localField)std::memcpy(out,&localIndex,4);
 else {assert(f==&mouseField);++aimReads;std::memcpy(out,mouseWorld,8);}};
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};fixture.method_get_param=[](const MethodInfo* m,uint32_t index)->const Il2CppType*{assert(m==&spawnMethod);return index==0?&sourceType:&modifierType;};
 fixture.class_get_method_from_name=[](Il2CppClass* cls,const char* name,int count)->const MethodInfo*{return cls!=&mainClass||count!=0?nullptr:!std::strcmp(name,"get_MouseWorld")?&mouseGetter:!std::strcmp(name,"get_myPlayer")&&!localMissing?&localGetter:nullptr;};
 fixture.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{assert(m==&mouseGetter||m==&localGetter);return m==&localGetter?&localType:&vectorType;};
 fixture.method_get_param_count=[](const MethodInfo* m)->uint32_t{assert(m==&mouseGetter||m==&localGetter);return 0;};
 fixture.object_get_class=[](Il2CppObject* object){return object->klass;};
 fixture.runtime_invoke=[](const MethodInfo* m,void* object,void** args,Il2CppObject** exception)->Il2CppObject*{
  if(m==&limitMethod){assert(object&&args);++limitCalls;auto* point=static_cast<float*>(args[0]);if(limitFailure){*exception=&source;return nullptr;}point[0]=120;point[1]=90;return nullptr;}
  assert(!object&&!args);
  if(m==&localGetter){if(localException){*exception=&source;return nullptr;}localBox.value=localIndex;return &localBox.object;}
  assert(m==&mouseGetter);++aimReads;if(mouseException){*exception=&source;return nullptr;}
  std::memcpy(mouseBox.xy,mouseWorld,8);return &mouseBox.object;
 };
 std::string error;Entry entry{"independent","ExampleShotgun","Actual/ExampleShotgun.cs","",6230,7};
 assert(publish({entry},&error)&&hasPolicies("independent"));assert(bindAssembly("independent","independent.dll",&error));
 alignas(8) char playerBytes[128]{},itemBytes[128]{};int type=6230,who=2;std::memcpy(itemBytes+16,&type,4);std::memcpy(playerBytes+20,&who,4);
 int bodyWidth=20;float position[2]{100,80};std::memcpy(playerBytes+32,position,8);std::memcpy(playerBytes+40,&bodyWidth,4);
 expectedDirection=1;
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
 // Native-only Gun/Rocket policies do not wait for Mono and still synchronize before the original method.
 Entry gun{"native-only","ExampleGun","ExampleGun.cs","",6231,0};assert(publish({gun},&error));
 assert(!hasPolicies("native-only"));failQueue=true;assert(bindAssembly("native-only","unused.dll",&error));failQueue=false;
 type=6231;std::memcpy(itemBytes+16,&type,4);warmState=0;int callbacks=callbackCalls;
 expectedDirection=-1;mouseWorld[0]=50;use(p,2,i,18,true,&useMethod);assert(callbackCalls==callbacks&&spawned.size()==1);spawned.clear();
 expectedDirection=1;mouseWorld[0]=150;use(p,2,i,18,true,&useMethod);assert(callbackCalls==callbacks&&spawned.size()==1);spawned.clear();
 // Vertical aim, remote ownership and invalid coordinates must never flip a player.
 mouseWorld[0]=110;use(p,2,i,18,true,&useMethod);spawned.clear();
 localIndex=3;mouseWorld[0]=0;const int reads=aimReads;use(p,2,i,18,true,&useMethod);assert(aimReads==reads);spawned.clear();localIndex=2;
 mouseWorld[0]=std::numeric_limits<float>::quiet_NaN();use(p,2,i,18,true,&useMethod);spawned.clear();
 mouseWorld[0]=50;bodyWidth=0;std::memcpy(playerBytes+40,&bodyWidth,4);use(p,2,i,18,true,&useMethod);spawned.clear();
 bodyWidth=20;std::memcpy(playerBytes+40,&bodyWidth,4);mouseWorld[0]=150;warmState=1;
 // Main.MouseWorld is also supported as a static Vector2 property on Android.
 originalUse=nullptr;originalSpawn=nullptr;mouseProperty=true;assert(prepare(&error));
 expectedDirection=-1;mouseWorld[0]=50;use(p,2,i,18,true,&useMethod);spawned.clear();
 mouseException=true;mouseWorld[0]=200;use(p,2,i,18,true,&useMethod);spawned.clear();mouseException=false;
 originalUse=nullptr;originalSpawn=nullptr;mouseProperty=false;assert(prepare(&error));
 expectedDirection=1;mouseWorld[0]=200;use(p,2,i,18,true,&useMethod);spawned.clear();
 localException=true;mouseWorld[0]=0;use(p,2,i,18,true,&useMethod);spawned.clear();localException=false;
 // Native cursor spawning uses the engine's ref Vector2 limiter before the factory.
 Entry cursorStaff{"cursor","IndependentStaff","Staff.cs","",6232,0,true};assert(publish({cursorStaff},&error));
 type=6232;std::memcpy(itemBytes+16,&type,4);mouseWorld[0]=2000;mouseWorld[1]=4000;
 use(p,2,i,18,true,&useMethod);assert(spawned.size()==1&&spawned[0].x==120&&spawned[0].y==90&&limitCalls==1);spawned.clear();
 localIndex=3;use(p,2,i,18,true,&useMethod);assert(spawned.size()==1&&spawned[0].x==100&&spawned[0].y==200&&limitCalls==1);spawned.clear();localIndex=2;
 limitFailure=true;use(p,2,i,18,true,&useMethod);assert(spawned.empty());limitFailure=false;
 unload("cursor");mouseWorld[1]=100;
 // Unregistered/vanilla items preserve direction even when local aim points the other way.
 type=1;std::memcpy(itemBytes+16,&type,4);mouseWorld[0]=0;use(p,2,i,18,true,&useMethod);spawned.clear();
 // Every bind is transactional and rejects malformed vector layouts and readonly direction.
 originalUse=nullptr;originalSpawn=nullptr;
 directionField.flags=0x20;assert(!prepare(&error));directionField.flags=0;
 yField.offset=0;assert(!prepare(&error));yField.offset=4;
 mouseField.type.cls=&integerClass;assert(!prepare(&error));mouseField.type.cls=&vectorClass;
 localMissing=true;assert(!prepare(&error));localMissing=false;
 localGetter.instance=true;assert(!prepare(&error));localGetter.instance=false;
 localType.cls=&floatClass;assert(!prepare(&error));localType.cls=&integerClass;
 // An inherited instance field does not replace a real static getter.
 localProperty=false;localField.flags=0;assert(prepare(&error));originalUse=nullptr;originalSpawn=nullptr;
 localMissing=true;assert(!prepare(&error));localMissing=false;localField.flags=0x10;
 assert(prepare(&error));originalUse=nullptr;originalSpawn=nullptr;localProperty=true;
 positionField.offset=124;assert(!prepare(&error));positionField.offset=32;
 xField.offset=sizeof(Il2CppObject);yField.offset=sizeof(Il2CppObject)+4;assert(prepare(&error));xField.offset=0;yField.offset=4;
 // Actual binder rejects mismatched widths, storage, bounds, factory ABI and method staticness.
 originalUse=nullptr;originalSpawn=nullptr;whoField.flags=0x10;assert(!prepare(&error));whoField.flags=0;
 whoField.offset=128;assert(!prepare(&error));whoField.offset=20;
 integerClass.size=2;assert(!prepare(&error));integerClass.size=4;
 modifierClass.value=true;assert(!prepare(&error)&&error.find("NativeAbiMismatch")!=std::string::npos);modifierClass.value=false;
 spawnMethod.instance=true;assert(!prepare(&error));spawnMethod.instance=false;
 missingMethod=true;assert(!prepare(&error));missingMethod=false;
 assert(prepare(&error));
 unload("native-only");assert(hasPolicies("independent"));assert(std::atomic_load(&policies)->count(6231)==0);
 type=6231;std::memcpy(itemBytes+16,&type,4);mouseWorld[0]=0;use(p,2,i,18,true,&useMethod);assert(!active.policy);
 unload("independent");assert(!hasPolicies("independent")&&std::atomic_load(&policies)->empty());
 std::cout<<"SHOOT_PRODUCTION_HOOKS_TRANSACTION_SOURCE_OWNER_AMMO_DAMAGE_AI_CANSHOOT_REENTRY_WARMUP_FAILURE_CONTRACTS_AND_COMPILED_SHOTGUN_AND_NATIVE_AIM_BEFORE_SHOOT_ANDROID_MYPLAYER_GETTER_PASS\n";
}
