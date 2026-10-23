#include "experiments/csharp/NativeContent.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;int rank=0;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct Projectile {Il2CppObject obj;int width=8,height=8,ai=0,penetrate=1,time=360,allocatedTrail=0;uint8_t friendly=0,hostile=0,ranged=1,ignore=1,tile=1,net=0;};
Il2CppClass integer{"System","Int32",4,true},boolean{"System","Boolean",1,true},single{"System","Single",4,true};
Il2CppClass intArray{"System","Int32[]",0,false,1,&integer},boolArray{"System","Boolean[]",0,false,1,&boolean};
Il2CppClass projectileClass{"Terraria","Projectile",sizeof(Projectile),false},itemClass{"Terraria","Item",64,false},sets{"","Sets",0,false};
struct Ints {Il2CppArray obj;int cells[1400]{};} lengths{{{&intArray,nullptr},nullptr,1400}},modes{{{&intArray,nullptr},nullptr,1400}};
struct Bools {Il2CppArray obj;uint8_t cells[1400]{};} golf{{{&boolArray,nullptr},nullptr,1400}};
bl::il2cpp::Api fixture;
std::map<std::string,FieldInfo> fields,tables;
std::atomic<int> reads{0};
int notifications=0,originals=0;
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& r){return r.ns=="Terraria.ID"?&sets:r.name=="Projectile"?&projectileClass:&itemClass;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){auto& m=cls==&sets?tables:fields;auto it=m.find(std::string(name));return it==m.end()?nullptr:&it->second;}
}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::csharp {void notifyManagedSourceProjectileDefaults(Il2CppObject*){++notifications;}}
namespace bl::runtime {void finishModProjectile(Il2CppObject*,int) {}}
std::string descriptor(const std::string& path,const std::string& prefix) {
    std::ifstream input(path);assert(input.good());std::string line;
    while(std::getline(input,line))if(line.starts_with(prefix))return line.substr(prefix.size());
    assert(false && "Compiled ExampleMod record missing");return {};
}
int resolve(const std::string& key){return key=="original/ExampleGolfBallProjectile"?1200:-1;}
int main(int argc,char** argv){
    assert(argc==2);
    fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
    fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
    fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
    fixture.class_get_name=[](Il2CppClass* c){return c->name;};
    fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
    fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return static_cast<int32_t>(c->size);};
    fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};
    fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
    fixture.field_static_get_value=[](FieldInfo* f,void* out){
        ++reads;
        Il2CppArray* array=f==&tables.at("TrailCacheLength")?&lengths.obj:f==&tables.at("TrailingMode")?&modes.obj:&golf.obj;
        std::memcpy(out,&array,sizeof array);
    };
    fields={{"netImportant",{{&boolean},offsetof(Projectile,net)}},{"ranged",{{&boolean},offsetof(Projectile,ranged)}}};
    tables={{"IsAGolfBall",{{&boolArray},0,0x10}},{"TrailCacheLength",{{&intArray},0,0x10}},{"TrailingMode",{{&intArray},0,0x10}}};
    using namespace bl::csharp;
    NativeDefaultsRequest request{"ExampleGolfBallProjectile","original","Content/Projectiles/ExampleGolfBallProjectile.cs",
        descriptor(argv[1],"NATIVE_PROJECTILE\tExampleGolfBallProjectile\t"),1200,true};
    std::string error;
    assert(publishNativeDefaults({request},&error,resolve));
    assert(golf.cells[1200]==0 && lengths.cells[1200]==0);
    pumpNativeStaticDefaultsOnGameThread();
    assert(golf.cells[1200]==1 && lengths.cells[1200]==20);
    const int installedReads=reads;
    pumpNativeStaticDefaultsOnGameThread();assert(reads==installedReads);
    // The three static values must be available to the engine BEFORE it allocates
    // oldPos/oldRot. Exercise the production hook, not a mirrored initializer.
    NativeProjectileDefaults defaults{};
    std::istringstream input(descriptor(argv[1],"PROJECTILE\tExampleGolfBallProjectile\t"));
    int friendly=0,minion=0,tile=0,water=0;float slots=0;
    input>>defaults.width>>defaults.height>>defaults.aiStyle>>defaults.penetrate>>defaults.timeLeft>>friendly>>minion>>slots>>tile>>water;
    assert(input && defaults.width==7 && defaults.aiStyle==149 && defaults.timeLeft==3600);
    defaults.friendly=friendly;defaults.minion=minion;defaults.minionSlots=slots;defaults.tileCollide=tile;defaults.ignoreWater=water;
    publishProjectileDefinition(1200,defaults,"original/ExampleGolfBallProjectile");
    g_refs.projAiStyle=offsetof(Projectile,ai);g_refs.projFriendly=offsetof(Projectile,friendly);
    g_refs.projHostile=offsetof(Projectile,hostile);g_refs.projRanged=offsetof(Projectile,ranged);
    g_refs.projPenetrate=offsetof(Projectile,penetrate);g_refs.projTimeLeft=offsetof(Projectile,time);
    g_refs.projIgnoreWater=offsetof(Projectile,ignore);g_refs.projTileCollide=offsetof(Projectile,tile);
    g_refs.projWidth=offsetof(Projectile,width);g_refs.projHeight=offsetof(Projectile,height);
    g_originalProjectile=[](Il2CppObject* obj,int32_t type,const MethodInfo*){
        ++originals;assert(notifications==originals);
        if(type==1200){assert(golf.cells[type]==1 && lengths.cells[type]==20 && modes.cells[type]==0);
            reinterpret_cast<Projectile*>(obj)->allocatedTrail=lengths.cells[type];}
    };
    // Simular recriação de tabelas depois da publicação: o hook também deve
    // reaplicar a política antes de entrar no original.
    lengths.cells[1200]=0;golf.cells[1200]=0;modes.cells[1200]=2;
    Projectile instance{{&projectileClass,nullptr}};
    hkProjectileDefaults(&instance.obj,1200,nullptr);
    assert(instance.allocatedTrail==20 && instance.width==7 && instance.height==7 && instance.ai==149);
    assert(instance.net==1 && instance.ranged==0 && instance.penetrate==-1 && instance.time==3600);
    assert(instance.friendly==1 && instance.tile==0 && instance.ignore==0);
    // Another mod receives independent cells; vanilla and previous IDs survive.
    auto other=request;other.type=1201;other.mod="other";other.descriptor="ProjectileSet_TrailCacheLength:i:8";
    assert(publishNativeDefaults({other},&error,resolve));
    lengths.cells[1]=99;applyNativeStaticDefaults(true,1201);
    assert(lengths.cells[1]==99 && lengths.cells[1200]==20 && lengths.cells[1201]==8);
    const auto published=std::atomic_load(&g_nativeDefaults);
    auto reject=[&](std::string wire,const char* reason){
        auto bad=request;bad.type=1202;bad.descriptor=std::move(wire);
        assert(!publishNativeDefaults({bad},&error,resolve));
        assert(error.find(reason)!=std::string::npos && std::atomic_load(&g_nativeDefaults)==published);
    };
    reject("ProjectileSet_TrailCacheLength:i:129","ValueInvalid");
    reject("ProjectileSet_TrailCacheLength:i:-1","ValueInvalid");
    reject("ProjectileSet_TrailingMode:i:4","ValueInvalid");
    reject("ProjectileSet_IsAGolfBall:i:1","ValueInvalid");
    reject("ProjectileSet_TrailCacheLength:i:20;ProjectileSet_TrailCacheLength:i:21","UnsupportedField");
    reject("ProjectileSet_Unknown:i:20","UnsupportedField");
    auto& lengthField=tables.at("TrailCacheLength");
    const int previousReads=reads;
    lengthField.type.cls=&integer;reject("ProjectileSet_TrailCacheLength:i:20","Missing");
    assert(reads==previousReads); // A scalar must never be read as an array pointer.
    lengthField.type.cls=&boolArray;reject("ProjectileSet_TrailCacheLength:i:20","ArrayMismatch");
    lengthField.type.cls=&intArray;lengthField.flags=0;reject("ProjectileSet_TrailCacheLength:i:20","Missing");lengthField.flags=0x10;
    lengths.obj.length=1202;reject("ProjectileSet_TrailCacheLength:i:20","ArrayMismatch");lengths.obj.length=1400;
    // Atomic batch failure must not publish the valid first row either.
    other.type=1203;auto bad=request;bad.type=1204;bad.descriptor="ProjectileSet_TrailCacheLength:i:999";
    assert(!publishNativeDefaults({other,bad},&error,resolve) && std::atomic_load(&g_nativeDefaults)==published);
    // Replaced arrays are revalidated when applying the policy.
    lengths.obj.obj.klass=&boolArray;lengths.cells[1201]=41;
    applyNativeStaticDefaults(true,1201);assert(lengths.cells[1201]==41);
    lengths.obj.obj.klass=&intArray;
    // No binding for a vanilla projectile: the original still runs untouched.
    hkProjectileDefaults(&instance.obj,1,nullptr);assert(originals==2);
    // Ownership, selective unload and same-ID reload preserve other mods.
    auto conflicting=request;conflicting.mod="foreign";
    assert(!publishNativeDefaults({conflicting},&error,resolve) && error.find("ContentOwnerMismatch")!=std::string::npos);
    auto revision=request;revision.descriptor="ProjectileSet_TrailCacheLength:i:10";
    assert(publishNativeDefaults({revision},&error,resolve));
    pumpNativeStaticDefaultsOnGameThread();
    assert(golf.cells[1200]==0 && modes.cells[1200]==0 && lengths.cells[1200]==10);
    assert(lengths.cells[1201]==8); // Reapplied active policy after a table publication.
    retireNativeDefaultsForMod("original");
    assert(lengths.cells[1200]==10); // The worker queues restoration, it never writes the game.
    pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1200]==0 && lengths.cells[1201]==8);
    instance.net=0;applyNativeDefaults(&instance.obj,true,1200);assert(instance.net==0);
    // Reload before the next pump must first retire the previous cell values.
    assert(publishNativeDefaults({request},&error,resolve));pumpNativeStaticDefaultsOnGameThread();
    retireNativeDefaultsForMod("original");
    assert(publishNativeDefaults({request},&error,resolve));pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1200]==20 && golf.cells[1200]==1);
    retireNativeDefaultsForMod("original");pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1200]==0 && golf.cells[1200]==0);
    // If another consumer has changed a retired cell, preserve that newer value.
    lengths.cells[1201]=57;retireNativeDefaultsForMod("other");pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1201]==57);
    // A temporarily unavailable table is retried without retaining an array pointer.
    assert(publishNativeDefaults({request},&error,resolve));pumpNativeStaticDefaultsOnGameThread();
    retireNativeDefaultsForMod("original");lengths.obj.length=1;
    pumpNativeStaticDefaultsOnGameThread();assert(!g_retryNativeStaticRestores.empty());
    lengths.obj.length=1400;pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1200]==0 && g_retryNativeStaticRestores.empty() && g_nativeStaticCells.empty());
    // Recompilar removendo o último NATIVE_* deve retirar o lote antigo.
    assert(publishNativeDefaults({request},&error,resolve,"original"));pumpNativeStaticDefaultsOnGameThread();
    auto wrongOwner=request;wrongOwner.mod="foreign";
    auto beforeReplace=std::atomic_load(&g_nativeDefaults);
    assert(!publishNativeDefaults({wrongOwner},&error,resolve,"original") && std::atomic_load(&g_nativeDefaults)==beforeReplace);
    assert(publishNativeDefaults({},&error,resolve,"original"));pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1200]==0 && golf.cells[1200]==0 && g_nativeStaticCells.empty());
    // Publicações e retiradas concorrentes não podem perder o valor original.
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for(int revision=0;revision<1000;++revision) {
            auto concurrent=request;concurrent.type=1206;concurrent.mod="parallel";
            concurrent.descriptor="ProjectileSet_TrailCacheLength:i:"+std::to_string(revision%2?8:20);
            std::string threadError;assert(publishNativeDefaults({concurrent},&threadError,resolve));
            if(revision%13==0)retireNativeDefaultsForMod("parallel");
        }
        done.store(true,std::memory_order_release);
    });
    do {
        pumpNativeStaticDefaultsOnGameThread();
        assert(lengths.cells[1206]==0 || lengths.cells[1206]==8 || lengths.cells[1206]==20);
    } while(!done.load(std::memory_order_acquire));
    producer.join();retireNativeDefaultsForMod("parallel");pumpNativeStaticDefaultsOnGameThread();
    assert(lengths.cells[1206]==0 && g_nativeStaticCells.empty());
    std::cout<<"GOLF_BALL_PRODUCTION_HOOK_AND_TYPED_STATIC_TABLES=PASS\n";
    std::cout<<"NATIVE_DEFAULTS_SELECTIVE_UNLOAD_RELOAD_AND_CELL_RESTORATION=PASS\n";
}
