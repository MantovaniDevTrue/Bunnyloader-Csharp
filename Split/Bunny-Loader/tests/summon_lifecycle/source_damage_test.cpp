#include "experiments/csharp/NativeContent.cpp"
#include <cassert>
#include <cstdarg>
#include <iostream>
struct Il2CppClass {};
struct MethodInfo {};
struct ItemFixture {Il2CppObject obj;int damage;};
struct ProjectileFixture {Il2CppObject obj;int type=1136,damage=45,original=0;};
struct SourceFixture {Il2CppObject obj;Il2CppObject* item;};
bl::il2cpp::Api fixtureApi;
namespace bl::il2cpp {
Api& api(){return fixtureApi;}
FieldInfo* findField(Il2CppClass*,std::string_view){return nullptr;}
Signature parseSignature(std::string_view){return {};}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature&,bool*,const MethodInfo**){return nullptr;}
std::string describeException(Il2CppObject*){return "FixtureException";}
}
namespace bl::log {void write(int,const char*,...) {}}
int main() {
 using namespace bl::csharp;
 g_refs.projectileType=offsetof(ProjectileFixture,type);g_refs.projectileDamage=offsetof(ProjectileFixture,damage);
 g_refs.itemDamage=offsetof(ItemFixture,damage);g_originalDamageField=offsetof(ProjectileFixture,original);
 g_projectiles.emplace(1136,NativeProjectileDefaults{});
 Il2CppClass sourceClass;ItemFixture item{{nullptr,nullptr},30};
 SourceFixture source{{&sourceClass,nullptr},&item.obj};ProjectileFixture projectile{{nullptr,nullptr}};
 g_projectileSourceItems.emplace(&sourceClass,ProjectileSourceItem{offsetof(SourceFixture,item),nullptr});
 // Execute the actual production source hook, preserving scaled damage and storing item baseline.
 hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);
 assert(projectile.original==30&&projectile.damage==45);
 item.damage=90;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==30&&projectile.damage==45);
 projectile.original=0;projectile.damage=75;hkProjectileStatsFromSource(&projectile.obj,nullptr,nullptr);
 assert(projectile.original==75&&projectile.damage==75);
 // Zero damage without a source remains zero; never steal another registered weapon's damage.
 projectile.original=0;projectile.damage=0;hkProjectileStatsFromSource(&projectile.obj,nullptr,nullptr);assert(projectile.original==0&&projectile.damage==0);
 projectile.original=99;projectile.damage=0;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==99&&projectile.damage==0);
 projectile.original=0;projectile.damage=55;projectile.type=1;hkProjectileStatsFromSource(&projectile.obj,&source.obj,nullptr);assert(projectile.original==0&&projectile.damage==55);
 std::cout<<"PROJECTILE_PRODUCTION_STATS_SOURCE_WEAPON_BASELINE_SCALED_DAMAGE_NO_SOURCE_ZERO_AND_FOREIGN_TYPES_PASS\n";
}
