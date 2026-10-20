#include "experiments/csharp/NativeShooting.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "content/projectiles/ModProjectiles.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"
#include "il2cpp/GuardedCall.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

namespace bl::csharp::shooting {
namespace {
struct Policy {Entry entry;mutable std::atomic<int> ready{0};mutable std::atomic<uint64_t> nextLog{0},suppressed{0};};
using Table=std::map<int,std::shared_ptr<Policy>>;
std::shared_ptr<const Table> policies=std::make_shared<const Table>();
std::mutex writer;
struct Refs {int type=-1,who=-1;FieldInfo* pool=nullptr;Il2CppClass* projectile=nullptr;const MethodInfo* time=nullptr;} refs;
using UseFn=void(*)(Il2CppObject*,int,Il2CppObject*,int,bool,const MethodInfo*);
using SpawnFn=int(*)(Il2CppObject*,float,float,float,float,int,int,float,int,float,float,float,Il2CppObject*,const MethodInfo*);
UseFn originalUse=nullptr;SpawnFn originalSpawn=nullptr;
struct Active {std::shared_ptr<Policy> policy;int player=0,weaponDamage=0,slots=0;};
thread_local Active active;
std::atomic<uint32_t> seed{0x9e3779b9};
struct Shot {std::string key;int type=0,damage=0;float x=0,y=0,vx=0,vy=0,kb=0,a0=0,a1=0,a2=0;};
struct Result {bool original=false;Shot modified;std::vector<Shot> additional;};
std::vector<std::string> split(const std::string& text,char sep) {
    std::vector<std::string> result;size_t start=0;
    for(size_t i=0;i<=text.size();++i)if(i==text.size()||text[i]==sep){result.push_back(text.substr(start,i-start));start=i+1;}
    return result;
}
bool integer(const std::string& text,int* value) {auto r=std::from_chars(text.data(),text.data()+text.size(),*value);return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size();}
bool real(const std::string& text,float* value) {char* end=nullptr;*value=std::strtof(text.c_str(),&end);return !text.empty()&&end==text.c_str()+text.size()&&std::isfinite(*value);}
void diagnostic(const Policy& policy,const char* stage,const std::string& reason) {
    const auto now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    auto expected=policy.nextLog.load(std::memory_order_relaxed);
    if(now<expected||!policy.nextLog.compare_exchange_strong(expected,now+5000)){policy.suppressed.fetch_add(1);return;}
    const bool pending=reason.find("DeadlineExceeded")!=std::string::npos||reason.find("NotReady")!=std::string::npos||
        reason.find("QueueBusy")!=std::string::npos||reason.find("QueueCapacityExceeded")!=std::string::npos;
    bl::log::write(pending?ANDROID_LOG_WARN:ANDROID_LOG_ERROR,"[CSharp][%s][Shoot] Mod=%s Class=%s File=%s ContentId=%d Stage=%s Result=ShotRejected Suppressed=%llu Detail=%s",pending?"WARN":"ERROR",
        policy.entry.mod.c_str(),policy.entry.name.c_str(),policy.entry.source.c_str(),policy.entry.type,stage,
        static_cast<unsigned long long>(policy.suppressed.exchange(0)),reason.c_str());
}
// Android 1.4.5.8.6 uses 1000 as the factory's no-slot return, also used by the
// original ItemLoader/GlobalItemLoader/PlayerItemHooks. A padded array is not this limit.
constexpr int kAndroidNoProjectileSlot=1000;
int poolSize() {
    Il2CppArray* pool=nullptr;auto& a=il2cpp::api();a.field_static_get_value(refs.pool,&pool);
    return pool&&pool->obj.klass&&a.class_get_rank(pool->obj.klass)==1&&a.class_get_element_class(pool->obj.klass)==refs.projectile&&
        pool->length>=kAndroidNoProjectileSlot&&pool->length<=10000?kAndroidNoProjectileSlot:0;
}
bool shot(const std::string& text,const Entry& entry,Shot* result,std::string* error) {
    auto f=split(text,',');Shot s;
    if(f.size()!=10||f[0].size()<2){*error="Stage=DecodeShot Expected=10Fields Reason=InvalidShotWire";return false;}
    if(f[0][0]=='v') {
        if(!integer(f[0].substr(1),&s.type)){*error="Stage=ResolveProjectile Reason=InvalidNumericType";return false;}
    } else if(f[0][0]=='m') {
        s.key=f[0].substr(1);auto slash=s.key.find('/');
        if(slash!=std::string::npos&&s.key.find('/',slash+1)!=std::string::npos){*error="Reason=InvalidProjectileKey";return false;}
        const auto mod=slash==std::string::npos?entry.mod:s.key.substr(0,slash);
        const auto name=slash==std::string::npos?s.key:s.key.substr(slash+1);
        s.type=runtime::modProjectileTypeByName(mod,name);
        if(s.type<0){*error="Stage=ResolveProjectile Dependency="+mod+"/"+name+" Reason=ProjectileDependencyMissing";return false;}
    } else {*error="Stage=ResolveProjectile Reason=UnknownProjectileToken";return false;}
    if(s.type<0||s.type>=runtime::projectileTypeCount()||!real(f[1],&s.x)||!real(f[2],&s.y)||!real(f[3],&s.vx)||!real(f[4],&s.vy)||
       !integer(f[5],&s.damage)||!real(f[6],&s.kb)||!real(f[7],&s.a0)||!real(f[8],&s.a1)||!real(f[9],&s.a2)||
       s.damage<0||s.damage>1000000||s.kb<0||s.kb>10000||std::fabs(s.x)>1e8||std::fabs(s.y)>1e8||std::fabs(s.vx)>4096||std::fabs(s.vy)>4096||
       std::fabs(s.a0)>1e8||std::fabs(s.a1)>1e8||std::fabs(s.a2)>1e8) {
        *error="Stage=ValidateShot Expected=RegisteredTypeFiniteBoundedStats Actual="+text.substr(0,512)+" Reason=InvalidShotOutput";return false;
    }
    *result=std::move(s);return true;
}
bool decode(const std::string& text,const Entry& entry,Result* result,std::string* error) {
    if(text.size()>32768){*error="Reason=ShootResponseCapacityExceeded";return false;}
    auto f=split(text,'|');Result staged;
    if(f.size()!=4||f[0]!="SHOT1"||(f[1]!="0"&&f[1]!="1")){*error=text.substr(0,4096);return false;}
    staged.original=f[1]=="1";
    if(!shot(f[2],entry,&staged.modified,error))return false;
    if(!f[3].empty()) {
        const auto records=split(f[3],';');if(records.size()>64){*error="Reason=SpawnCapacityExceeded Expected=AtMost64";return false;}
        for(const auto& record:records){Shot s;if(!shot(record,entry,&s,error))return false;staged.additional.push_back(std::move(s));}
    }
    *result=std::move(staged);return true;
}
std::string snapshot(const Active& context,const Shot& s) {
    char buffer[640];const auto n=std::snprintf(buffer,sizeof buffer,"%d,%d,%d,%u,%d,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g,%.9g,%.9g",
        context.player,context.policy->entry.type,context.weaponDamage,seed.fetch_add(0x9e3779b9)&0x7fffffffu,
        s.type,s.x,s.y,s.vx,s.vy,s.damage,s.kb,s.a0,s.a1,s.a2);
    return n>0&&static_cast<size_t>(n)<sizeof buffer?std::string(buffer,static_cast<size_t>(n)):std::string();
}
bool ready(const Policy& policy) {
    int state=policy.ready.load(std::memory_order_acquire);if(state==1)return true;if(state<0)return false;
    std::string error;state=managedShootWarmupState(policy.entry.mod,policy.entry.assembly,policy.entry.name,&error);
    if(state==1){policy.ready.store(1,std::memory_order_release);BL_INFO("[CSharp][Shoot] Mod=%s Class=%s Stage=Warmup Result=Ready",policy.entry.mod.c_str(),policy.entry.name.c_str());}
    else {if(state<0)policy.ready.store(-1,std::memory_order_release);diagnostic(policy,"Warmup",state<0?error:"Reason=ManagedDispatcherNotReady");}
    return state==1;
}
void use(Il2CppObject* player,int index,Il2CppObject* item,int damage,bool feedback,const MethodInfo* method) {
    if(!originalUse)return;
    const auto table=std::atomic_load_explicit(&policies,std::memory_order_acquire);
    int type=0;if(item)std::memcpy(&type,reinterpret_cast<char*>(item)+refs.type,4);
    auto found=table->find(type);
    if(!player||found==table->end()){originalUse(player,index,item,damage,feedback,method);return;}
    auto policy=found->second;int who=-1;std::memcpy(&who,reinterpret_cast<char*>(player)+refs.who,4);
    Active context{policy,who,damage,poolSize()};
    bool allowed=context.slots>0&&who>=0&&who<=255&&damage>=0&&damage<=1000000;
    if(!allowed)diagnostic(*policy,"ReadSnapshot","Expected=Player0..255,Damage0..1000000,PoolWithAtLeast1000Cells ActualPlayer="+std::to_string(who)+" ActualDamage="+std::to_string(damage)+" ValidatedProjectileCapacity="+std::to_string(context.slots)+" Reason=InvalidNativeShotSnapshot");
    else allowed=ready(*policy);
    if(allowed&&(policy->entry.mask&1)) {
        std::string output;const bool invoked=invokeManagedShoot(policy->entry.mod,policy->entry.assembly,policy->entry.name,0,snapshot(context,{}),&output);
        allowed=invoked&&output=="1";
        if(!invoked||(output!="0"&&output!="1"))diagnostic(*policy,"CanShoot",output);
    }
    if(!allowed){reinterpret_cast<void(*)(Il2CppObject*,Il2CppObject*,const MethodInfo*)>(il2cpp::methodPointer(refs.time))(player,item,refs.time);return;}
    const auto previous=active;active=std::move(context);
    // Native exceptions must not leave a stale thread-local shooting scope armed.
    il2cpp::Caught caught;
    auto run=[&]{originalUse(player,index,item,damage,feedback,method);};
    const bool completed=il2cpp::guarded(run,&caught);active=previous;
    if(!completed)diagnostic(*policy,"ItemCheck_Shoot","Reason=NativeException Detail="+il2cpp::describeCaught(caught));
}
int emit(const Policy& policy,const Shot& s,Il2CppObject* source,int owner,Il2CppObject* modifier,const MethodInfo* method,int sentinel) {
    int slot=sentinel;il2cpp::Caught caught;
    auto run=[&]{slot=originalSpawn(source,s.x,s.y,s.vx,s.vy,s.type,s.damage,s.kb,owner,s.a0,s.a1,s.a2,modifier,method);};
    if(!il2cpp::guarded(run,&caught))diagnostic(policy,"NewProjectile","Reason=NativeException Detail="+il2cpp::describeCaught(caught));
    return slot;
}
int spawn(Il2CppObject* source,float x,float y,float vx,float vy,int type,int damage,float kb,int owner,float a0,float a1,float a2,Il2CppObject* modifier,const MethodInfo* method) {
    if(!active.policy)return originalSpawn(source,x,y,vx,vy,type,damage,kb,owner,a0,a1,a2,modifier,method);
    const auto context=active;active={}; // Own spawns must traverse the remaining hooks without recursively invoking Shoot.
    const int sentinel=context.slots;
    Shot input;input.type=type;input.damage=damage;input.x=x;input.y=y;input.vx=vx;input.vy=vy;input.kb=kb;input.a0=a0;input.a1=a1;input.a2=a2;
    if(!(context.policy->entry.mask&6)){const int slot=emit(*context.policy,input,source,owner,modifier,method,sentinel);active=context;return slot;}
    std::string output,error;Result result;
    if(!invokeManagedShoot(context.policy->entry.mod,context.policy->entry.assembly,context.policy->entry.name,1,snapshot(context,input),&output)||
       !decode(output,context.policy->entry,&result,&error)) {
        diagnostic(*context.policy,"Shoot",error.empty()?output:error);active=context;return sentinel;
    }
    // No output touches Terraria until the entire response, including dependencies, passes validation.
    for(const auto& s:result.additional)
        emit(*context.policy,s,source,owner,modifier,method,sentinel);
    int slot=sentinel;
    if(result.original)slot=emit(*context.policy,result.modified,source,owner,modifier,method,sentinel);
    active=context;return slot;
}
bool prepare(std::string* error) {
    if(originalUse&&originalSpawn)return true;
    using namespace il2cpp;auto& a=api();auto* player=findClass({"Terraria","Player",{}});auto* item=findClass({"Terraria","Item",{}});
    auto* projectile=findClass({"Terraria","Projectile",{}});auto* main=findClass({"Terraria","Main",{}});Refs staged;staged.projectile=projectile;
    if(!player||!item||!projectile||!main){*error="Expected=Terraria.Player/Item/Projectile/Main Reason=RequiredNativeClassMissing";return false;}
    if(!nativeFieldContract(item,"type","System.Int32",false,4,nullptr,&staged.type,error)||
       !nativeFieldContract(player,"whoAmI","System.Int32",false,4,nullptr,&staged.who,error))return false;
    auto* pool=main?findField(main,"projectile"):nullptr;
    auto* array=pool?a.class_from_il2cpp_type(a.field_get_type(pool)):nullptr;
    if(!array||!(a.field_get_flags(pool)&0x10u)||!a.class_get_rank||!a.class_get_element_class||!a.field_static_get_value||
       a.class_get_rank(array)!=1||a.class_get_element_class(array)!=projectile){*error="Field=Main.projectile Expected=StaticProjectileArray Actual=MissingOrWrongType Reason=NativeArrayContractMismatch";return false;}
    staged.pool=pool;
    auto method=[&](Il2CppClass* cls,const char* signature,bool instance)->const MethodInfo* {
        bool ambiguous=false;const MethodInfo* renamed=nullptr;
        auto* found=findMethodBySignature(cls,parseSignature(signature),&ambiguous,&renamed);
        if(!found&&!ambiguous)found=renamed; // Names may drift; the complete type/return contract still has to match uniquely.
        if(!found||!a.method_is_instance||a.method_is_instance(found)!=instance){*error="Expected="+std::string(signature)+" Reason=NativeMethodContractMismatch";return nullptr;}
        return found;
    };
    auto* useMethod=method(player,"void ItemCheck_Shoot(int i, Item sItem, int weaponDamage, bool withAudioVisualFeedback)",true);
    auto* spawnMethod=method(projectile,"int NewProjectile(IEntitySource spawnSource, float X, float Y, float SpeedX, float SpeedY, int Type, int Damage, float KnockBack, int Owner, float ai0, float ai1, float ai2, NewProjectileModifier modifer)",false);
    staged.time=method(player,"void ApplyItemTime(Item sItem)",true);
    if(!useMethod||!spawnMethod||!staged.time)return false;
    if(!methodPointer(staged.time)){*error="Method=ApplyItemTime Reason=NativeMethodPointerMissing";return false;}
    // Both non-primitive parameters in the factory must be references, never value-type aggregates passed by a different ABI.
    if(!a.method_get_param||!a.class_from_il2cpp_type){*error="Reason=RequiredMethodTypeExportsMissing";return false;}
    for(const int index:{0,12}) {
        auto* cls=a.class_from_il2cpp_type(a.method_get_param(spawnMethod,index));
        if(!cls||a.class_is_valuetype(cls)){*error="Method=NewProjectile Parameter="+std::to_string(index)+" Expected=NativeReference Actual=MissingOrValueType Reason=NativeAbiMismatch";return false;}
    }
    refs=staged;
    if((!originalUse&&!hook::install(useMethod,use,&originalUse))||(!originalSpawn&&!hook::install(spawnMethod,spawn,&originalSpawn))) {
        *error="Reason=NativeShootHookInstallFailed";return false;
    }
    return true;
}
}
bool publish(const std::vector<Entry>& entries,std::string* error) {
    if(entries.empty())return true;
    std::lock_guard lock(writer);
    if(!prepare(error)){*error="[CSharp][ERROR][Shoot] Mod="+entries.front().mod+" Class="+entries.front().name+" File="+entries.front().source+" Stage=BindNative Result=BatchRejected Detail="+*error;return false;}
    auto next=std::make_shared<Table>(*std::atomic_load_explicit(&policies,std::memory_order_acquire));std::map<int,bool> seen;
    for(const auto& entry:entries){
        if(entry.type<1||entry.mask<1||entry.mask>7||!seen.emplace(entry.type,true).second){*error="[CSharp][ERROR][Shoot] Mod="+entry.mod+" Class="+entry.name+" Stage=Publish Reason=InvalidOrDuplicateShootOwner Result=BatchRejected";return false;}
        auto policy=std::make_shared<Policy>();policy->entry=entry;(*next)[entry.type]=std::move(policy);
    }
    std::atomic_store_explicit(&policies,std::shared_ptr<const Table>(std::move(next)),std::memory_order_release);return true;
}
bool hasPolicies(const std::string& mod) {auto table=std::atomic_load_explicit(&policies,std::memory_order_acquire);for(const auto& p:*table)if(p.second->entry.mod==mod)return true;return false;}
bool bindAssembly(const std::string& mod,const std::string& assembly,std::string* error) {
    std::lock_guard lock(writer);auto next=std::make_shared<Table>(*std::atomic_load_explicit(&policies,std::memory_order_acquire));
    for(auto& p:*next)if(p.second->entry.mod==mod) {
        auto replacement=std::make_shared<Policy>();replacement->entry=p.second->entry;replacement->entry.assembly=assembly;
        if(!queueManagedShootWarmup(mod,assembly,replacement->entry.name)){*error="[CSharp][ERROR][Shoot] Mod="+mod+" Class="+replacement->entry.name+" Stage=QueueWarmup Reason=WorkerQueueCapacityExceeded Result=Rejected";return false;}
        BL_INFO("[CSharp][Shoot] Mod=%s Class=%s File=%s Stage=Warmup Result=Queued",mod.c_str(),replacement->entry.name.c_str(),replacement->entry.source.c_str());
        p.second=std::move(replacement);
    }
    std::atomic_store_explicit(&policies,std::shared_ptr<const Table>(std::move(next)),std::memory_order_release);return true;
}
}
