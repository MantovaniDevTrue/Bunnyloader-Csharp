#include "experiments/csharp/NativeContent.cpp"
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>
#include <barrier>
#include <fstream>
#include <sstream>
#include <limits>
struct Il2CppClass {};
struct MethodInfo {};
struct ItemFixture {Il2CppObject obj;int damage;};
struct ProjectileFixture {Il2CppObject obj;int type=1136,damage=45,original=0;
 int ai=0,penetrate=1,time=360,width=8,height=8,maxPenetrate=1;float scale=1;uint8_t friendly=1,hostile=0,ranged=1,ignore=1,tile=1;};
struct SourceFixture {Il2CppObject obj;Il2CppObject* item;};
bl::il2cpp::Api fixtureApi;
namespace bl::il2cpp {
Api& api(){return fixtureApi;}
FieldInfo* findField(Il2CppClass*,std::string_view){return nullptr;}
Signature parseSignature(std::string_view){return {};}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature&,bool*,const MethodInfo**){return nullptr;}
std::string describeException(Il2CppObject*){return "FixtureException";}
}
int defaultsNotifications=0,originalDefaultsCalls=0;
namespace bl::csharp {void notifyManagedSourceProjectileDefaults(Il2CppObject*){++defaultsNotifications;assert(originalDefaultsCalls<defaultsNotifications);}}
int itemReservations=0;
std::map<std::string,int> reservedItems;
namespace bl::runtime {
bool modProjectileBelongsTo(int type,const std::string& mod){return type==1136&&mod=="fixture";}
int registerModItem(ModItemDef def){
 ++itemReservations;
 assert(def.mod=="fixture"&&def.texture=="staff.png"&&!def.names.empty());
 const auto key=def.mod+"/"+def.name;
 if(reservedItems.contains(key))return -1;
 return reservedItems[key]=6230+static_cast<int>(reservedItems.size());
}
int modItemTypeByKey(const std::string& key){auto it=reservedItems.find(key);return it==reservedItems.end()?-1:it->second;}
int modProjectileTypeByName(const std::string& mod,const std::string& name){return mod=="fixture"&&name=="ExampleMinion"?1136:-1;}
void finishModProjectile(Il2CppObject* object,int){
 auto& p=*reinterpret_cast<ProjectileFixture*>(object);p.width=static_cast<int>(p.width*p.scale);p.height=static_cast<int>(p.height*p.scale);p.maxPenetrate=p.penetrate;
}}
namespace bl::log {void write(int,const char*,...) {}}
int main(int argc,char** argv) {
 using namespace bl::csharp;
 g_refs.projectileType=offsetof(ProjectileFixture,type);g_refs.projectileDamage=offsetof(ProjectileFixture,damage);
 g_refs.itemDamage=offsetof(ItemFixture,damage);g_originalDamageField=offsetof(ProjectileFixture,original);
 g_projectiles.emplace(1136,NativeProjectileDefaults{8,8,1,1,360,true,false});
 publishProjectileDefinition(1136,g_projectiles.at(1136),"fixture/Minion");
 Il2CppClass sourceClass;ItemFixture item{{nullptr,nullptr},30};
 SourceFixture source{{&sourceClass,nullptr},&item.obj};ProjectileFixture projectile{{nullptr,nullptr}};
 g_projectileSourceItems.emplace(&sourceClass,ProjectileSourceItem{offsetof(SourceFixture,item),nullptr});
 // Execute the actual production source hook, preserving scaled damage and storing item baseline.
 hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);
 assert(projectile.original==30&&projectile.damage==45);
 item.damage=90;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==30&&projectile.damage==45);
 projectile.original=0;projectile.damage=75;hkProjectileStatsFromSource(&projectile.obj,nullptr,nullptr);
 assert(projectile.original==75&&projectile.damage==75);
 item.damage=0;projectile.original=0;projectile.damage=45;
 hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==0&&projectile.damage==45);
 // Zero damage without a source remains zero; never steal another registered weapon's damage.
 projectile.original=0;projectile.damage=0;hkProjectileStatsFromSource(&projectile.obj,nullptr,nullptr);assert(projectile.original==0&&projectile.damage==0);
 projectile.original=99;projectile.damage=0;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==99&&projectile.damage==0);
 projectile.original=0;projectile.damage=55;projectile.type=1;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==0&&projectile.damage==55);
 // Execute the actual native SetDefaults hook: notify before the engine can reset whoAmI,
 // including transitions from a managed projectile to an unregistered type.
 g_originalProjectile=[](Il2CppObject*,int32_t,const MethodInfo*){++originalDefaultsCalls;assert(defaultsNotifications==originalDefaultsCalls);};
 hkProjectileDefaults(&projectile.obj,1,nullptr);
 assert(defaultsNotifications==1&&originalDefaultsCalls==1);
 // Native defaults precede the same finalization used by real projectiles:
 // scale changes geometry, and penetration initializes maxPenetrate.
 g_refs.projAiStyle=offsetof(ProjectileFixture,ai);g_refs.projPenetrate=offsetof(ProjectileFixture,penetrate);g_refs.projTimeLeft=offsetof(ProjectileFixture,time);
 g_refs.projWidth=offsetof(ProjectileFixture,width);g_refs.projHeight=offsetof(ProjectileFixture,height);
 g_refs.projFriendly=offsetof(ProjectileFixture,friendly);g_refs.projHostile=offsetof(ProjectileFixture,hostile);g_refs.projRanged=offsetof(ProjectileFixture,ranged);
 g_refs.projIgnoreWater=offsetof(ProjectileFixture,ignore);g_refs.projTileCollide=offsetof(ProjectileFixture,tile);
 auto bindings=std::make_shared<NativeDefaultsTable>();NativeDefaultWrite scaleWrite{offsetof(ProjectileFixture,scale),std::vector<uint8_t>(4)},penetrateWrite{offsetof(ProjectileFixture,penetrate),std::vector<uint8_t>(4)};
 float scale=2;int penetrate=-1;std::memcpy(scaleWrite.bytes.data(),&scale,4);std::memcpy(penetrateWrite.bytes.data(),&penetrate,4);
 (*bindings)[{true,1136}]={scaleWrite,penetrateWrite};std::atomic_store(&g_nativeDefaults,std::shared_ptr<const NativeDefaultsTable>(bindings));
 projectile.type=1136;hkProjectileDefaults(&projectile.obj,1136,nullptr);
 assert(projectile.width==16&&projectile.height==16&&projectile.maxPenetrate==-1);
 std::cout<<"PROJECTILE_NATIVE_DEFAULTS_BEFORE_GEOMETRY_AND_PENETRATION_FINALIZATION_PASS\n";
 // The real publisher must expose one coherent immutable revision to game readers.
 NativeProjectileDefaults initial{};initial.width=1;initial.frames=1;
 publishProjectileDefinition(1136,initial,"fixture/0");
 std::atomic<bool> stop{false},broken{false};std::atomic<int> reads{0};std::barrier start{4};
 std::vector<std::thread> readers;
 for(int i=0;i<3;++i)readers.emplace_back([&]{
   start.arrive_and_wait();
   do {
     const auto view=std::atomic_load_explicit(&g_projectileView,std::memory_order_acquire);
     const auto& record=view->at(1136);const int revision=std::stoi(record.key.substr(8));
     if(record.defaults.width!=revision%500+1||record.defaults.frames!=revision%4+1)broken.store(true);
     ++reads;
   } while(!stop.load());
 });
 start.arrive_and_wait();
 for(int revision=1;revision<=2000;++revision) {
   auto next=initial;next.width=revision%500+1;next.frames=revision%4+1;
   publishProjectileDefinition(1136,next,"fixture/"+std::to_string(revision));
 }
 stop.store(true);for(auto& reader:readers)reader.join();assert(!broken.load()&&reads.load()>=3);
 std::cout<<"PROJECTILE_SNAPSHOT_CONCURRENT_PUBLICATION_PASS revisions=2000 readers=3\n";
 std::cout<<"PROJECTILE_PRODUCTION_STATS_SOURCE_WEAPON_BASELINE_SCALED_DAMAGE_NO_SOURCE_ZERO_AND_FOREIGN_TYPES_PASS\n";
 // Run the production weapon registration branch using the real compiled staff
 // descriptor, rather than only checking bootstrap parsing or a mirrored rule.
 assert(argc==2);std::ifstream wire(argv[1]);assert(wire.good());
 std::string row,staffRow;
 while(std::getline(wire,row))if(row.rfind("WEAPON\tExampleMinionItem\t",0)==0)staffRow=row;
 assert(!staffRow.empty());std::istringstream descriptor(staffRow);
 std::string tag,name,target;int summon=0;
 NativeContentSpec spec{};spec.kind=NativeWeapon;
 descriptor>>tag>>name>>spec.damage>>spec.useTime>>spec.shootSpeed>>target>>summon;
 assert(name=="ExampleMinionItem"&&target=="ExampleMinion"&&summon==1);
 assert(spec.damage==30&&spec.useTime==36&&spec.shootSpeed==0.f);
 spec.name=name.c_str();spec.shootType=1136;
 NativeRegisterContext ctx{};ctx.modId="fixture";ctx.sourceFile="content/ExampleMinionItem.cs";
 const int staff=registerNativeWeapon(ctx,spec,"staff.png","Example Minion Staff");
 assert(staff>=0&&ctx.items==1&&ctx.lastError.empty()&&itemReservations==1);
 auto view=std::atomic_load(&g_weaponView);
 assert(view->at(staff).shootSpeed==0.f&&view->at(staff).shootType==1136);
 // Apply the same zero-speed value through the real single managed-default path.
 g_ready=true;ManagedProjectileDefaults projectileDefaults{8,8,1,1,360,true};
 ManagedWeaponDefaults weaponDefaults{30,36,0.f};
 assert(applyManagedNativeDefaults("fixture","ExampleMinion",1136,projectileDefaults,name,staff,weaponDefaults));
 assert(std::atomic_load(&g_weaponView)->at(staff).shootSpeed==0.f);
 const auto accepted=std::atomic_load(&g_weaponView);
 for(float invalid:{-0.01f,40.01f,std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
   spec.shootSpeed=invalid;weaponDefaults.shootSpeed=invalid;
   assert(registerNativeWeapon(ctx,spec,"staff.png","Staff")==-1);
   assert(ctx.lastError.find("WeaponShootSpeedOutOfRange")!=std::string::npos);
   assert(ctx.lastError.find("FiniteShootSpeed:0..40")!=std::string::npos);
   assert(!applyManagedNativeDefaults("fixture","ExampleMinion",1136,projectileDefaults,name,staff,weaponDefaults));
   assert(ctx.items==1&&itemReservations==1&&std::atomic_load(&g_weaponView)==accepted);
 }
 assert(validWeaponShootSpeed(0.f)&&validWeaponShootSpeed(0.5f)&&validWeaponShootSpeed(40.f));
 spec.shootSpeed=0;spec.shootType=99;
 assert(registerNativeWeapon(ctx,spec,"staff.png","Staff")==-1);
 assert(ctx.lastError.find("WeaponProjectileOwnershipMismatch")!=std::string::npos&&itemReservations==1);
 spec.shootType=1136;spec.damage=0;
 assert(registerNativeWeapon(ctx,spec,"staff.png","Staff")==-1);
 assert(ctx.lastError.find("WeaponDamageOutOfRange")!=std::string::npos&&itemReservations==1);
 spec.damage=30;spec.useTime=1;
 assert(registerNativeWeapon(ctx,spec,"staff.png","Staff")==-1);
 assert(ctx.lastError.find("WeaponUseTimeOutOfRange")!=std::string::npos&&itemReservations==1);
 spec.useTime=36;
 assert(registerNativeWeapon(ctx,spec,"staff.png","Staff")==-1);
 assert(ctx.lastError.find("NativeItemReservationRejected")!=std::string::npos&&ctx.items==1);
 assert(std::atomic_load(&g_weaponView)==accepted);
 std::cout<<"PRODUCTION_WEAPON_REGISTRATION_AND_MANAGED_DEFAULTS_ZERO_SPEED_COMPILED_STAFF_PASS\n";

}
