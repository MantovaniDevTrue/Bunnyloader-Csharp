// Run the production binder against metadata without Projectile.identity.
#include "experiments/csharp/NativeSummonRuntime.h"
#include <cassert>
#include <iostream>
#include <map>

struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {bool instance;};
bl::il2cpp::Api bindingApi;
bl::runtime::GameRefs bindingGame;
Il2CppClass mainClass{"Terraria","Main",128,false},playerClass{"Terraria","Player",128,false},
    projectileClass{"Terraria","Projectile",128,false},npcClass{"Terraria","NPC",128,false},collisionClass{"Terraria","Collision",128,false};
std::map<Il2CppClass*,std::map<std::string,FieldInfo>> members;
unsigned identityLookups=0;
MethodInfo instanceMethod{true},staticMethod{false};
namespace bl::il2cpp {
Api& api(){return bindingApi;}
Il2CppClass* findClass(const TypeRef& type) {
    for(auto* cls:{&mainClass,&playerClass,&projectileClass,&npcClass,&collisionClass})if(type.name==cls->name)return cls;
    return nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(name=="identity")++identityLookups;
    auto& fields=members[cls];auto found=fields.find(std::string(name));return found==fields.end()?nullptr:&found->second;
}
Signature parseSignature(std::string_view text){Signature sig;sig.name=text;return sig;}
const MethodInfo* findMethodBySignature(Il2CppClass* cls,const Signature&,bool*,const MethodInfo**){return cls==&collisionClass?&staticMethod:&instanceMethod;}
std::string describeMethod(const MethodInfo*){return {};}
}
namespace bl::runtime {GameRefs& game(){return bindingGame;}}
namespace bl::log {void write(int,const char*,...) {}}

int main() {
    using namespace bl::csharp::nativeSummon;
    Il2CppClass boolean{"System","Boolean",1,true},integer{"System","Int32",4,true},single{"System","Single",4,true},
        vector{"Microsoft.Xna.Framework","Vector2",8,true},ints{"System","Int32[]",8,false},
        players{"Terraria","Player[]",8,false},projectiles{"Terraria","Projectile[]",8,false};
    bindingApi.class_from_il2cpp_type=[](const Il2CppType* type){return type->cls;};
    bindingApi.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    bindingApi.field_get_flags=[](FieldInfo* f){return f->flags;};
    bindingApi.field_get_offset=[](FieldInfo* f){return f->offset;};
    bindingApi.class_get_name=[](Il2CppClass* c){return c->name;};
    bindingApi.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    bindingApi.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    bindingApi.class_instance_size=[](Il2CppClass* c){return c->size;};
    bindingApi.class_value_size=[](Il2CppClass* c,uint32_t* alignment){*alignment=4;return static_cast<int32_t>(c->size);};
    bindingApi.method_is_instance=[](const MethodInfo* m){return m->instance;};
    members[&mainClass]={{"netMode",{{&integer},0,0x10}},{"player",{{&players},0,0x10}},{"projectile",{{&projectiles},0,0x10}}};
    members[&playerClass]={{"active",{{&boolean},16}},{"dead",{{&boolean},17}},{"buffType",{{&ints},24}},{"buffTime",{{&ints},32}},
        {"position",{{&vector},40}},{"width",{{&integer},48}},{"height",{{&integer},52}},{"direction",{{&integer},56}}};
    members[&projectileClass]={{"whoAmI",{{&integer},16}},{"position",{{&vector},20}},{"velocity",{{&vector},28}},
        {"width",{{&integer},36}},{"height",{{&integer},40}},{"active",{{&boolean},44}},{"rotation",{{&single},48}},
        {"type",{{&integer},52}},{"owner",{{&integer},56}},{"timeLeft",{{&integer},60}},{"damage",{{&integer},64}},
        {"friendly",{{&boolean},68}},{"spriteDirection",{{&integer},72}},{"originalDamage",{{&integer},76}}};
    auto& e=bindingGame.entity;e.whoAmI=16;e.position=20;e.velocity=28;e.width=36;e.height=40;
    auto& p=bindingGame.proj;p.active=44;p.rotation=48;p.type=52;p.owner=56;p.timeLeft=60;p.damage=64;
    std::string error;assert(prepareSummon(&error)&&g_summon.ready&&identityLookups==0);
    assert(g_summon.owner.manualTarget==-1&&g_summon.originalDamage==76);
    // A field never read by the bridge must not be bound, even if another version declares it differently.
    g_summon={};members[&projectileClass]["identity"]={{&boolean},80};
    assert(prepareSummon(&error)&&identityLookups==0);
    auto rejected=[&](const char* reason) {
        g_summon={};error.clear();assert(!prepareSummon(&error)&&!g_summon.ready);
        assert(g_summon.originalDamage==-1&&error.find(reason)!=std::string::npos);
    };
    auto& who=members[&projectileClass]["whoAmI"];
    who.type.cls=&boolean;rejected("Field=whoAmI");who.type.cls=&integer;
    who.offset=127;rejected("InvalidFieldBounds");who.offset=16;
    who.flags=0x10;rejected("NativeTypeOrStorageMismatch");who.flags=0x20;rejected("ReadOnly");who.flags=0;
    e.whoAmI=20;rejected("ActualOffset=16");e.whoAmI=16;
    members[&projectileClass].erase("originalDamage");rejected("Field=originalDamage");
    assert(identityLookups==0);
    std::cout<<"SUMMON_PRODUCTION_BINDER_NO_IDENTITY_OPTIONAL_MANUAL_TARGET_TYPED_SLOT_BOUNDS_STORAGE_AND_REQUIRED_DAMAGE_PASS\n";
}
