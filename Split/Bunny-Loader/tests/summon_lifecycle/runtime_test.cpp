// Execute the production hook with controlled IL2CPP objects/providers, no QuickJS.
#include "experiments/csharp/ManagedProjectileHook.cpp"
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <thread>

struct Il2CppClass {const char* ns;const char* name;int size;Il2CppClass* element=nullptr;};
struct FieldInfo {int id;};
struct MethodInfo {int id;};
struct ArrayInts {Il2CppArray header;int32_t data[8]{};};
struct Owner {Il2CppObject obj;bool active=true,dead=false;Il2CppArray* types;Il2CppArray* times;bl::csharp::Vector2 position{};int width=20,height=40,direction=1,manual=-1;};
struct Proj {Il2CppObject obj;int slot=0;bl::csharp::Vector2 position{},velocity{};int width=18,height=28;bool active=true;float rotation=0;int type=1136,owner=0,time=18000,damage=45;bool friendly=true;int sprite=1,original=30;bool minion=true;};
struct Npc {Il2CppObject obj;int slot=0;bl::csharp::Vector2 position{},velocity{};int width=20,height=20;bool active=true,friendly=false,invulnerable=false;int life=100;};
struct Objects {Il2CppArray header;Il2CppObject* entries[200]{};};
bl::il2cpp::Api fakeApi;bl::runtime::GameRefs fakeGame;
Il2CppClass intClass{"System","Int32",4},boolClass{"System","Boolean",1},intArrayClass{"System","Int32[]",0,&intClass};
Il2CppClass ownerClass{"Terraria","Player",sizeof(Owner)},projClass{"Terraria","Projectile",sizeof(Proj)},npcClass{"Terraria","NPC",sizeof(Npc)};
Il2CppClass playersClass{"Terraria","Player[]",0,&ownerClass},projectilesClass{"Terraria","Projectile[]",0,&projClass},npcsClass{"Terraria","NPC[]",0,&npcClass};
ArrayInts types{{{&intArrayClass,nullptr},nullptr,8}},times{{{&intArrayClass,nullptr},nullptr,8}};
Owner owner{{&ownerClass,nullptr},true,false,&types.header,&times.header};
Owner otherOwner{{&ownerClass,nullptr},true,false,&types.header,&times.header};
Proj projectile{{&projClass,nullptr}};
Npc enemy{{&npcClass,nullptr}};
Objects players{{{&playersClass,nullptr},nullptr,2}},projectiles{{{&projectilesClass,nullptr},nullptr,1000}},npcs{{{&npcsClass,nullptr},nullptr,200}};
FieldInfo playersField{1},projectilesField{2},npcsField{3},netField{4};
MethodInfo addBuff{1},kill{2},chase{3},sight{4};
struct BoolBox {Il2CppObject obj;uint8_t value=1;} yes{{&boolClass,nullptr}},no{{&boolClass,nullptr},0};
int adds=0,kills=0,killCallbacks=0,workerTicks=0,vanillaCalls=0,spawnTickets=0;bool visible=true;bl::csharp::SummonMovement configured;float seenOwnerX=0;
namespace bl::il2cpp {
Api& api(){return fakeApi;}
Il2CppClass* findClass(const TypeRef&){return nullptr;}
FieldInfo* findField(Il2CppClass*,std::string_view){return nullptr;}
Signature parseSignature(std::string_view){return {};}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature&,bool*,const MethodInfo**){return nullptr;}
std::string describeMethod(const MethodInfo*){return {};}
std::string describeException(Il2CppObject*){return "FixtureException";}
}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::runtime {GameRefs& game(){return fakeGame;}bool resolveGameRefs(){return true;}}
namespace bl::csharp {
bool managedEntitySourceModsRegistered(){return true;}
bool hasManagedEntityProjectileType(int type){return type==1136;}
ProjectilePolicy nativeManagedProjectilePolicy(int){return {1,5,1,700,true};}
SummonMovement nativeManagedMinionMovement(int){return configured;}
bool nativeManagedProjectileFields(int,ProjectileGameplayFields* fields){fields->minion=offsetof(Proj,minion);return true;}
std::string nativeManagedProjectileKey(int){return "fixture/IndependentMinion";}
int nativeManagedMinionBuff(int){return 403;}
TerrariaSnapshot captureTerrariaSnapshot(bool,bool){assert(false&&"A minion must never follow LocalPlayer");return {};}
bool tickManagedEntityProjectile(int,int,int,int,uintptr_t,float,float,float,float,float px,float,ManagedEntityVelocity*,bool initial,int,float,float){++workerTicks;if(initial)++spawnTickets;seenOwnerX=px;return false;}
bool queueManagedEntityCombatEvent(int kind,int,int,int,int,uintptr_t,int,int){if(kind==5)++killCallbacks;return true;}
void noteManagedLivePerfProjectileCurveUpdate(){}
void noteManagedLivePerfProjectileVelocityWrite(){}
bool invokeManagedProjectileAI(int,int,int*){return false;}
}
int main() {
 using namespace bl::csharp;
 fakeApi.class_get_rank=[](const Il2CppClass* c){return c->element?1:0;};
 fakeApi.class_get_element_class=[](Il2CppClass* c){return c->element;};
 fakeApi.class_get_name=[](Il2CppClass* c){return c->name;};fakeApi.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fakeApi.class_is_valuetype=[](const Il2CppClass* c){return c==&boolClass||c==&intClass;};
 fakeApi.class_value_size=[](Il2CppClass* c,uint32_t* alignment){*alignment=4;return c->size;};
 fakeApi.field_static_get_value=[](FieldInfo* field,void* out){
    if(field->id==4){*static_cast<int*>(out)=0;return;}
    *static_cast<Il2CppArray**>(out)=field->id==1?&players.header:field->id==2?&projectiles.header:&npcs.header;
 };
 fakeApi.runtime_invoke=[](const MethodInfo* method,void* object,void** args,Il2CppObject** error)->Il2CppObject* {
    *error=nullptr;
    if(method->id==1){++adds;types.data[0]=*static_cast<int*>(args[0]);times.data[0]=*static_cast<int*>(args[1]);return nullptr;}
    if(method->id==2){++kills;queueManagedSourceProjectileCombat(5,static_cast<Il2CppObject*>(object),-1,0);projectile.active=false;return nullptr;}
    return method->id==4&&!visible?&no.obj:&yes.obj;
 };
 auto& e=fakeGame.entity;e.whoAmI=offsetof(Proj,slot);e.position=offsetof(Proj,position);e.velocity=offsetof(Proj,velocity);e.width=offsetof(Proj,width);e.height=offsetof(Proj,height);
 auto& p=fakeGame.proj;p.active=offsetof(Proj,active);p.type=offsetof(Proj,type);p.owner=offsetof(Proj,owner);p.timeLeft=offsetof(Proj,time);p.rotation=offsetof(Proj,rotation);p.damage=offsetof(Proj,damage);
 g_summon.ready=true;g_summon.netMode=&netField;g_summon.players=&playersField;g_summon.projectiles=&projectilesField;g_summon.playerClass=&ownerClass;g_summon.projectileClass=&projClass;
 g_summon.owner={offsetof(Owner,active),offsetof(Owner,dead),offsetof(Owner,types),offsetof(Owner,times),offsetof(Owner,position),offsetof(Owner,width),offsetof(Owner,height),offsetof(Owner,direction),offsetof(Owner,manual)};
 g_summon.friendly=offsetof(Proj,friendly);g_summon.spriteDirection=offsetof(Proj,sprite);g_summon.originalDamage=offsetof(Proj,original);
 g_summon.addBuff=&addBuff;g_summon.kill=&kill;g_summon.canChase=&chase;g_summon.canHit=&sight;
 g_targets.ready=true;g_targets.npcArray=&npcsField;g_targets.npcClass=&npcClass;g_targets.active=offsetof(Npc,active);g_targets.friendly=offsetof(Npc,friendly);g_targets.dontTakeDamage=offsetof(Npc,invulnerable);g_targets.life=offsetof(Npc,life);
 g_managedEntityNetMode=&netField;players.entries[0]=&owner.obj;players.entries[1]=&otherOwner.obj;projectiles.entries[0]=&projectile.obj;
 projectiles.header.length=1; // Real length matches actual allocated fixture.
 std::string error;assert(parseSummonMovement("4,8,12,80,20,10,-48,600,20,0.82,60,2000,40,40,1200,0.05,0.04",&configured,&error));
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);
 assert(adds==1&&kills==0&&projectile.time==2&&projectile.damage==45&&spawnTickets==1);
 const int firstGeneration=g_managedEntityProjectileSlots[0].generation;
 // A temporary sample using the same slot cannot reset a live projectile.
 Proj sample=projectile;notifyManagedSourceProjectileDefaults(&sample.obj);
 assert(g_managedEntityProjectileSlots[0].generation==firstGeneration);
 std::thread contentWorker([]{notifyManagedSourceProjectileDefaults(&projectile.obj);});contentWorker.join();
 assert(g_managedEntityProjectileSlots[0].generation==firstGeneration);
 projectile.time=18000;
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);
 assert(adds==1&&spawnTickets==1&&g_managedEntityProjectileSlots[0].generation==firstGeneration);
 types.data[0]=0;times.data[0]=0;projectile.time=18000;
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);
 assert(kills==1&&killCallbacks==1&&adds==1&&!projectile.active);
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(kills==1&&killCallbacks==1);
 // Actual native pool reuse: same object, slot, type, owner and timeLeft. No identity field.
 projectile.active=true;notifyManagedSourceProjectileDefaults(&projectile.obj);projectile.time=2;
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(adds==2&&projectile.active&&spawnTickets==2&&g_managedEntityProjectileSlots[0].generation!=firstGeneration);
 owner.dead=true;runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(kills==2&&killCallbacks==2);owner.dead=false;
 projectile.active=true;notifyManagedSourceProjectileDefaults(&projectile.obj);projectile.owner=1;otherOwner.position={500,100};
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(seenOwnerX==510&&projectile.active);
 // Far from owner: recover position, never chase a cached target indefinitely.
 projectile.position={-5000,-5000};runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);
 assert(std::fabs(projectile.position.x-(510-10-9))<.01f&&std::fabs(projectile.position.y-(120-48-14))<.01f);
 // Follow a moving owner over hundreds of real production ticks.
 for(int i=0;i<600;++i){otherOwner.position.x+=2;projectile.position.x+=projectile.velocity.x;projectile.position.y+=projectile.velocity.y;runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);}
 assert(std::fabs(projectile.position.x-otherOwner.position.x)<200);
 enemy.position={otherOwner.position.x+180,100};npcs.entries[0]=&enemy.obj;
 Vector2 checked;assert(summonTargetPosition(0,&checked));assert(summonBool(&chase,&enemy.obj,nullptr,1136));assert(summonBool(&sight,nullptr,nullptr,1136));
 runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);

 assert(projectile.friendly&&g_managedEntityProjectileSlots[0].targetCache.target==0);
 visible=false;runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(!projectile.friendly);
 visible=true;enemy.position={10000,100};otherOwner.manual=0;runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(!projectile.friendly);
 otherOwner.active=false;runManagedEntityProjectile(&projectile.obj,fakeGame,1136,true);assert(kills==3&&killCallbacks==3);
 assert(workerTicks>600);
 // Existing native curve motion still runs alongside managed lifecycle/combat.
 configured.enabled=false;otherOwner.active=true;otherOwner.manual=-1;enemy.active=false;
 projectile.active=true;notifyManagedSourceProjectileDefaults(&projectile.obj);projectile.velocity={2,0};projectile.time=18000;
 g_curveNetMode=&netField;g_curveCount.store(1);g_curveStates[0].type=1136;
 g_curveStates[0].enabled.store(true);g_curveStates[0].turns.fill(20);g_curveByType[1136].store(1);
 g_original=[](Il2CppObject*,const MethodInfo*){++vanillaCalls;};
 const int previousTicks=workerTicks;hkAi(&projectile.obj,nullptr);
 assert(vanillaCalls==1&&workerTicks==previousTicks+1&&projectile.velocity.y>0.03f);
 assert(queueManagedSourceProjectileCombat(4,&projectile.obj,0,12));
 assert(queueManagedSourceProjectileCombat(5,&projectile.obj,-1,0));
 assert(!queueManagedSourceProjectileCombat(5,&projectile.obj,-1,0));
 std::cout<<"SUMMON_NO_IDENTITY_NATIVE_POOL_REUSE_TIMELEFT_REFRESH_SAMPLE_ISOLATION_PRODUCTION_HOOK_BUFF_CANCEL_DEATH_RESUMMON_OWNER_FOLLOW_TELEPORT_TARGET_LOS_LEASH_AND_DAMAGE_PRESERVATION_LEGACY_CURVE_CALLBACKS_PASS\n";
}
