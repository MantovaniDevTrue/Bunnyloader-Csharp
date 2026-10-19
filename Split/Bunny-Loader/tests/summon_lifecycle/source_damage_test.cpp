#include "experiments/csharp/NativeContent.cpp"
#include <cassert>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>
#include <barrier>
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
namespace bl::runtime {void finishModProjectile(Il2CppObject* object,int){
 auto& p=*reinterpret_cast<ProjectileFixture*>(object);p.width=static_cast<int>(p.width*p.scale);p.height=static_cast<int>(p.height*p.scale);p.maxPenetrate=p.penetrate;
}}
namespace bl::log {void write(int,const char*,...) {}}
int main() {
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
}
