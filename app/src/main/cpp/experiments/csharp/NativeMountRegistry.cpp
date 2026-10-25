#include "experiments/csharp/NativeMountRegistry.h"
#include "experiments/csharp/NativeMountVisualPolicy.h"
#include "experiments/csharp/NativeVisualRenderer.h"
#include "hook/HookManager.h"
#include "experiments/csharp/NativeStaticValue.h"
#include "content/buffs/ModBuffs.h"
#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"
#include "core/Log.h"
#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace bl::csharp {
namespace {
using namespace il2cpp;
struct Slot {
    std::string mod,name;
    int type=-1,buff=-1;
    bool published=false;
    mount_policy::Definition policy{};
};
std::map<int,Slot> slots;
std::map<std::string,int> keys;
native_static::Value countValue;
native_static::Value mountsValue;
Il2CppClass* mountClass=nullptr;
Il2CppClass* dataClass=nullptr;
std::unique_ptr<runtime::TypeTables> mountSets;
struct AssetRoot {
    uint32_t handle=0;
    explicit AssetRoot(Il2CppObject* asset) {
        if(asset&&api().gchandle_new)handle=api().gchandle_new(asset,false);
    }
    ~AssetRoot() {if(handle&&api().gchandle_free)api().gchandle_free(handle);}
    Il2CppObject* get() const {
        return handle&&api().gchandle_get_target?api().gchandle_get_target(handle):nullptr;
    }
};
std::map<int,std::vector<std::shared_ptr<AssetRoot>>> assets;
struct BalloonState {int mount=-1;std::array<float,3> rotations{};uint32_t random=0;};
std::array<BalloonState,256> balloons{};
int mountKindOffset=-1,playerIndexOffset=-1,playerPositionOffset=-1,playerVelocityOffset=-1,playerWidthOffset=-1,playerHeightOffset=-1,playerBuffTypeOffset=-1,playerBuffTimeOffset=-1,playerMountOffset=-1;
const MethodInfo* newDust=nullptr;
const MethodInfo* setMountMethod=nullptr;
int gemDiamond=-1;
using SetMountFn=void(*)(Il2CppObject*,int32_t,Il2CppObject*,bool,const MethodInfo*);
using UpdateEffectsFn=void(*)(Il2CppObject*,Il2CppObject*,const MethodInfo*);
SetMountFn originalSetMount=nullptr;
UpdateEffectsFn originalUpdateEffects=nullptr;
bool effectsHooked=false;
std::string keyOf(const std::string& mod,const std::string& name) {return mod+"/"+name;}

int constantDustId(const char* name) {
    auto& a=api();
    auto* cls=findClass({"Terraria.ID","DustID",{}});
    auto* field=cls?findField(cls,name):nullptr;
    int value=-1;
    if(!field||!a.field_get_flags||!a.field_get_type||!a.class_from_il2cpp_type)return -1;
    auto* kind=a.class_from_il2cpp_type(a.field_get_type(field));
    const char* ns=kind?a.class_get_namespace(kind):nullptr;
    const char* type=kind?a.class_get_name(kind):nullptr;
    if(!ns||!type||std::strcmp(ns,"System"))return -1;
    const bool shortType=!std::strcmp(type,"Int16");
    if(!shortType&&std::strcmp(type,"Int32"))return -1;
    if(a.field_get_flags(field)&0x40u) {
        using Getter=const void*(*)(const FieldInfo*,const Il2CppType**);
        void* library=dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);
        const auto getter=library?reinterpret_cast<Getter>(
            dlsym(library,"il2cpp_field_get_default_value")):nullptr;
        const Il2CppType* metadata=nullptr;
        const auto* src=getter?getter(field,&metadata):nullptr;
        if(library)dlclose(library);
        if(!src)return -1;
        if(shortType){int16_t id=-1;std::memcpy(&id,src,sizeof(id));value=id;}
        else std::memcpy(&value,src,sizeof(value));
    } else if((a.field_get_flags(field)&0x10u)&&a.field_static_get_value) {
        if(shortType){int16_t id=-1;a.field_static_get_value(field,&id);value=id;}
        else a.field_static_get_value(field,&value);
    }
    return value>0&&value<2000?value:-1;
}
uint32_t randomBalloon(BalloonState& state,int index) {
    if(!state.random)state.random=0x9e3779b9u^(static_cast<uint32_t>(index+1)*0x85ebca6bu);
    state.random^=state.random<<13;
    state.random^=state.random>>17;
    state.random^=state.random<<5;
    return state.random;
}
struct Position {float x,y;};
void dustAt(float x,float y,int type,int width=2,int height=2) {
    if(!newDust||type<0||!std::isfinite(x)||!std::isfinite(y)||width<1||height<1)return;
    Position position{x,y};
    int32_t alpha=0,kind=type,dx=width,dy=height;
    uint32_t color=0xffffffffu;
    float speed=0.f,scale=1.f;
    void* args[]={&position,&dx,&dy,&kind,&speed,&speed,&alpha,&color,&scale};
    Il2CppObject* error=nullptr;
    api().runtime_invoke(newDust,nullptr,args,&error);
    if(error){static unsigned faults=0;if(faults++<3)BL_WARN("[CSharp][Mount] Stage=NewDust Result=Exception");}
}
int playerIndex(Il2CppObject* player) {
    if(!player||playerIndexOffset<0)return -1;
    int value=-1;
    std::memcpy(&value,reinterpret_cast<const char*>(player)+playerIndexOffset,sizeof(value));
    return value>=0&&value<256?value:-1;
}
int activeMountType(Il2CppObject* mount) {
    if(!mount||mountKindOffset<0)return -1;
    int value=-1;
    std::memcpy(&value,reinterpret_cast<const char*>(mount)+mountKindOffset,sizeof(value));
    return value;
}
bool playerLocation(Il2CppObject* player,Position* where,Position* velocity,int* width,int* height) {
    if(!player||playerPositionOffset<0||playerVelocityOffset<0||
       playerWidthOffset<0||playerHeightOffset<0)return false;
    const auto* src=reinterpret_cast<const char*>(player);
    std::memcpy(where,src+playerPositionOffset,sizeof(*where));
    std::memcpy(velocity,src+playerVelocityOffset,sizeof(*velocity));
    std::memcpy(width,src+playerWidthOffset,4);
    std::memcpy(height,src+playerHeightOffset,4);
    return std::isfinite(where->x)&&std::isfinite(where->y)&&
           std::isfinite(velocity->x)&&std::isfinite(velocity->y)&&
           *width>0&&*width<=128&&*height>0&&*height<=128;
}
void onSetMount(Il2CppObject* self,int32_t type,Il2CppObject* player,
                bool ignoreEffect,const MethodInfo* method) {
    const auto iter=slots.find(type);
    const bool custom=iter!=slots.end()&&iter->second.published&&iter->second.policy.kind==0;
    const bool entering=custom&&activeMountType(self)!=type;
    if(originalSetMount)originalSetMount(self,type,player,ignoreEffect||entering,method);
    if(!entering||!player||gemDiamond<0)return;
    const int index=playerIndex(player);
    if(index<0)return;
    Position where{},velocity{};
    int width=0,height=0;
    if(!playerLocation(player,&where,&velocity,&width,&height))return;
    balloons[static_cast<size_t>(index)].mount=type;
    balloons[static_cast<size_t>(index)].rotations={};
    const mount_visual::Point center{where.x+width*0.5f,where.y+height*0.5f};
    for(int i=0;i<16;++i){
        const auto point=mount_visual::ringDust(center,i);
        dustAt(point.x,point.y,gemDiamond,1,1);
    }
}
void onUpdateEffects(Il2CppObject* self,Il2CppObject* player,const MethodInfo* method) {
    if(originalUpdateEffects)originalUpdateEffects(self,player,method);
    if(!player)return;
    const int type=activeMountType(self);
    auto it=slots.find(type);
    if(it==slots.end()||!it->second.published)return;
    const int index=playerIndex(player);
    if(index<0)return;
    auto& state=balloons[static_cast<size_t>(index)];
    if(state.mount!=type){state.mount=type;state.rotations={};}
    Position where{},velocity{};
    int width=0,height=0;
    if(!playerLocation(player,&where,&velocity,&width,&height))return;
    const auto& policy=it->second.policy;
    if(policy.kind==0){
        std::array<float,3> samples{};
        for(auto& sample:samples)
            sample=static_cast<float>(randomBalloon(state,index)&0xffffu)/65535.f;
        mount_visual::updateBalloons(&state.rotations,velocity.x,samples,3);
        if(std::abs(velocity.x)>policy.runDustThreshold)
            dustAt(where.x,where.y,gemDiamond,width,height);
    } else if(mount_visual::minecartSparkle(randomBalloon(state,index))&&gemDiamond>0)
        dustAt(where.x+width*0.5f,where.y+height*0.5f,gemDiamond,1,1);
}
bool hookMountEffects(std::string* error) {
    if(effectsHooked)return true;
    auto& a=api();
    auto* player=findClass({"Terraria","Player",{}});
    std::string detail;
    const auto bind=[&](Il2CppClass* cls,const char* name,const char* type,size_t bytes,int* offset) {
        return nativeFieldContract(cls,name,type,false,bytes,nullptr,offset,&detail);
    };
    if(!bind(mountClass,"_type","System.Int32",4,&mountKindOffset)||
       !bind(player,"whoAmI","System.Int32",4,&playerIndexOffset)||
       !bind(player,"position","Microsoft.Xna.Framework.Vector2",8,&playerPositionOffset)||
       !bind(player,"velocity","Microsoft.Xna.Framework.Vector2",8,&playerVelocityOffset)||
       !bind(player,"width","System.Int32",4,&playerWidthOffset)||
       !bind(player,"height","System.Int32",4,&playerHeightOffset)||
       !bind(player,"mount","Terraria.Mount",sizeof(void*),&playerMountOffset)||
       !bind(player,"buffType","System.Int32[]",sizeof(void*),&playerBuffTypeOffset)||
       !bind(player,"buffTime","System.Int32[]",sizeof(void*),&playerBuffTimeOffset)) {
        *error="Mount native effects player contract: "+detail;return false;
    }
    auto* dust=findClass({"Terraria","Dust",{}});
    newDust=dust?findMethodBySignature(dust,parseSignature(
        "int NewDust(Vector2 Position, int Width, int Height, int Type, float SpeedX, float SpeedY, int Alpha, Color newColor, float Scale)")):nullptr;
    gemDiamond=constantDustId("GemDiamond");
    setMountMethod=findMethodBySignature(mountClass,parseSignature(
        "void SetMount(int m, Player mountedPlayer, bool ignoreEffect)"));
    auto* effect=findMethodBySignature(mountClass,parseSignature("void UpdateEffects(Player mountedPlayer)"));
    const bool dustReady=newDust&&!a.method_is_instance(newDust);
    const bool setReady=setMountMethod&&a.method_is_instance(setMountMethod);
    const bool updateReady=effect&&a.method_is_instance(effect);
    if(!dustReady||!setReady||!updateReady||gemDiamond<1) {
        *error="Mount native dust and lifecycle signatures unavailable: NewDust="+
            std::to_string(dustReady?1:0)+" SetMount="+
            std::to_string(setReady?1:0)+" UpdateEffects="+
            std::to_string(updateReady?1:0)+" GemDiamond="+
            std::to_string(gemDiamond);
        if(!dustReady)for(const auto& signature:listOverloads(dust,"NewDust"))
            BL_WARN("[CSharp][Mount] NativeDustCandidate=%s",signature.c_str());
        if(!setReady)for(const auto& signature:listOverloads(mountClass,"SetMount"))
            BL_WARN("[CSharp][Mount] NativeSetMountCandidate=%s",signature.c_str());
        if(!updateReady)for(const auto& signature:listOverloads(mountClass,"UpdateEffects"))
            BL_WARN("[CSharp][Mount] NativeUpdateEffectsCandidate=%s",signature.c_str());
        return false;
    }
    if(!hook::install(setMountMethod,onSetMount,&originalSetMount)||
       !hook::install(effect,onUpdateEffects,&originalUpdateEffects)) {
        *error="Mount native lifecycle hook install failed";return false;
    }
    effectsHooked=true;
    return true;
}
bool file(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>0&&st.st_size<1024*1024;
}
bool setNumber(Il2CppObject* data,Il2CppClass* cls,const char* name,
               const char* kind,size_t bytes,const void* value,std::string* error) {
    int offset=-1;
    if(!nativeFieldContract(cls,name,kind,false,bytes,nullptr,&offset,error))return false;
    std::memcpy(reinterpret_cast<char*>(data)+offset,value,bytes);
    return true;
}
bool setInt(Il2CppObject* data,Il2CppClass* cls,const char* name,int value,std::string* error) {
    return setNumber(data,cls,name,"System.Int32",4,&value,error);
}
bool setFloat(Il2CppObject* data,Il2CppClass* cls,const char* name,float value,std::string* error) {
    return setNumber(data,cls,name,"System.Single",4,&value,error);
}
bool setBool(Il2CppObject* data,Il2CppClass* cls,const char* name,bool value,std::string* error) {
    uint8_t bit=value?1:0;
    return setNumber(data,cls,name,"System.Boolean",1,&bit,error);
}
bool setObject(Il2CppObject* target,Il2CppClass* cls,const char* name,
               Il2CppObject* value,std::string* error) {
    auto& a=api();
    auto* f=cls?findField(cls,name):nullptr;
    if(!f||!a.field_get_flags||!a.field_get_type||!a.class_from_il2cpp_type||
       !a.class_is_valuetype||!a.field_get_offset||!a.class_instance_size||
       (a.field_get_flags(f)&0x10u)) {
        *error=std::string("MountData field unavailable: ")+name;
        return false;
    }
    auto* declared=a.class_from_il2cpp_type(a.field_get_type(f));
    auto* actual=value?a.object_get_class(value):nullptr;
    if(!declared||a.class_is_valuetype(declared)||
       (value&&(!actual||!a.class_is_assignable_from||
                !a.class_is_assignable_from(declared,actual)))) {
        *error=std::string("MountData field type mismatch: ")+name;
        return false;
    }
    const size_t offset=a.field_get_offset(f);
    if(offset<sizeof(Il2CppObject)||offset+sizeof(void*)>a.class_instance_size(cls)) {
        *error=std::string("MountData field bounds invalid: ")+name;
        return false;
    }
    auto** cell=reinterpret_cast<void**>(reinterpret_cast<char*>(target)+offset);
    if(a.gc_wbarrier_set_field)a.gc_wbarrier_set_field(target,cell,value);
    else *cell=value;
    return true;
}
bool preflight(std::string* error,bool requireArray=false) {
    auto& a=api();
    if(!a.runtime_invoke||!a.object_new||!a.array_new||!a.field_static_get_value||
       !a.field_static_set_value||!a.class_get_element_class||!a.class_get_rank||
       !a.gc_wbarrier_set_field||!a.class_is_assignable_from||
       !a.class_instance_size||!a.method_is_instance) {
        *error="Mount IL2CPP required exports missing";return false;
    }
    auto* ids=findClass({"Terraria.ID","MountID",{}});
    mountClass=findClass({"Terraria","Mount",{}});
    dataClass=mountClass?findNested(mountClass,"MountData"):nullptr;
    countValue=native_static::resolve(ids,"Count");
    mountsValue=native_static::resolve(mountClass,"mounts");
    int64_t n=0;
    if(!countValue||!countValue.writable()||
       !native_static::readInteger(countValue,&n)||n<1||n>2048||
       !mountsValue||!mountsValue.writable()||!dataClass) {
        *error="MountID.Count or Mount.mounts metadata unavailable";return false;
    }
    if(a.class_get_rank(mountsValue.type)!=1||
       a.class_get_element_class(mountsValue.type)!=dataClass) {
        *error="Mount.mounts declared type is not MountData[]";return false;
    }
    Il2CppArray* mounted=nullptr;
    if(!native_static::read(mountsValue,&mounted,sizeof(mounted))) {
        *error="Mount.mounts native static read failed";return false;
    }
    if(mounted&&(a.class_get_rank(mounted->obj.klass)!=1||
       mounted->length>4096||
       a.class_get_element_class(mounted->obj.klass)!=dataClass)) {
        *error="Mount.mounts runtime array type or capacity mismatch";return false;
    }
    if(requireArray&&!mounted) {
        *error="Mount.mounts pending vanilla initialization at publish";return false;
    }
    if(!mountSets) {
        mountSets=std::make_unique<runtime::TypeTables>("C# MountID.Sets",static_cast<int>(n),
             std::vector<runtime::TableClass>{{"Terraria.ID","MountID","Sets"}},
             runtime::TypeTables::Fill::Mode);
    }
    return true;
}
bool slotArray(int type,std::string* error) {
    auto& a=api();
    Il2CppArray* original=nullptr;
    if(!native_static::read(mountsValue,&original,sizeof(original))||!original)return false;
    if(type<0||type>=4096)return false;
    if(original->length<=static_cast<uintptr_t>(type)) {
        auto* larger=runtime::TypeTables::resizedCopy(original,static_cast<uintptr_t>(type+1));
        if(!larger||!native_static::write(mountsValue,&larger,sizeof(larger))) {
            *error="Mount.mounts growth failed";return false;
        }
    }
    mountSets->grow(type,type+1);
    mountSets->checkPending(type+1);
    auto* mountId=findClass({"Terraria.ID","MountID",{}});
    auto* sets=mountId?findNested(mountId,"Sets"):nullptr;
    auto cart=native_static::resolve(sets,"Cart");
    Il2CppArray* cartArray=nullptr;
    if(!cart||!native_static::read(cart,&cartArray,sizeof(cartArray))||
       !nativeArrayContract(cartArray,"Boolean",1,4096)||cartArray->length<=static_cast<uintptr_t>(type)) {
        *error="MountID.Sets.Cart unavailable or unexpanded";return false;
    }
    return true;
}
bool setMountData(int type,Il2CppObject* object,std::string* error) {
    Il2CppArray* arr=nullptr;
    if(!native_static::read(mountsValue,&arr,sizeof(arr))||!arr||
       static_cast<uintptr_t>(type)>=arr->length)return false;
    auto** items=static_cast<Il2CppObject**>(arrayData(arr));
    api().gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(arr),
        reinterpret_cast<void**>(&items[type]),object);
    return true;
}
bool writeCart(int type,bool cart,std::string* error) {
    auto* ids=findClass({"Terraria.ID","MountID",{}});
    auto* sets=ids?findNested(ids,"Sets"):nullptr;
    auto value=native_static::resolve(sets,"Cart");
    Il2CppArray* array=nullptr;
    if(!native_static::read(value,&array,sizeof(array))||
       !nativeArrayContract(array,"Boolean",1,4096)||array->length<=static_cast<uintptr_t>(type)) {
        *error="MountID.Sets.Cart cannot write";return false;
    }
    static_cast<uint8_t*>(arrayData(array))[type]=cart?1:0;
    return true;
}
bool writeMountBuff(int buff,int type,std::string* error) {
    auto* buffId=findClass({"Terraria.ID","BuffID",{}});
    auto* sets=buffId?findNested(buffId,"Sets"):nullptr;
    auto value=native_static::resolve(sets,"MountType");
    Il2CppArray* array=nullptr;
    if(!native_static::read(value,&array,sizeof(array))||
       !nativeArrayContract(array,"Int32",4,16384)||buff<0||
       static_cast<uintptr_t>(buff)>=array->length) {
        *error="BuffID.Sets.MountType unavailable or unexpanded";return false;
    }
    static_cast<int32_t*>(arrayData(array))[buff]=type;
    return true;
}
int vanillaMountId(const char* name) {
    auto& a=api();auto* ids=findClass({"Terraria.ID","MountID",{}});
    auto* field=ids?findField(ids,name):nullptr;
    if(!field||!a.field_get_flags||!a.field_get_type)return -1;
    auto* kind=a.class_from_il2cpp_type(a.field_get_type(field));
    const char* ns=kind?a.class_get_namespace(kind):nullptr;
    const char* type=kind?a.class_get_name(kind):nullptr;
    if(!ns||!type||std::strcmp(ns,"System"))return -1;
    const bool small=!std::strcmp(type,"Int16");
    if(!small&&std::strcmp(type,"Int32"))return -1;
    int id=-1;
    if(a.field_get_flags(field)&0x40u) {
        using Getter=const void*(*)(const FieldInfo*,const Il2CppType**);
        auto* getter=reinterpret_cast<Getter>(dlsym(RTLD_DEFAULT,"il2cpp_field_get_default_value"));
        const Il2CppType* metadata=nullptr;const void* src=getter?getter(field,&metadata):nullptr;
        if(!src)return -1;
        if(small){int16_t v=-1;std::memcpy(&v,src,2);id=v;}
        else std::memcpy(&id,src,4);
    } else if(a.field_get_flags(field)&0x10u) {
        if(small){int16_t v=-1;a.field_static_get_value(field,&v);id=v;}
        else a.field_static_get_value(field,&id);
    }
    return id>=0&&id<2048?id:-1;
}
bool copyMinecartDelegates(Il2CppObject* custom,std::string* error) {
    auto& a=api();
    const int meow=vanillaMountId("MeowmereMinecart");
    const int fart=vanillaMountId("FartMinecart");
    Il2CppArray* all=nullptr;
    if(meow<0||fart<0||!native_static::read(mountsValue,&all,sizeof(all))||
       !all||static_cast<uintptr_t>(std::max(meow,fart))>=all->length) {
        *error="Vanilla meow/fart minecart delegates missing";return false;
    }
    auto** items=static_cast<Il2CppObject**>(arrayData(all));
    auto* field=findField(dataClass,"delegations");
    if(!field||!a.field_get_type||!a.field_get_flags||!a.field_get_offset||
       (a.field_get_flags(field)&0x10u)||!items[meow]||!items[fart]) {
        *error="MountData.delegations field unavailable";return false;
    }
    auto* delegateClass=a.class_from_il2cpp_type(a.field_get_type(field));
    if(!delegateClass||a.class_is_valuetype(delegateClass)) {
        *error="Minecart delegates require reference type";return false;
    }
    const size_t offset=a.field_get_offset(field);
    if(offset<sizeof(Il2CppObject)||offset+sizeof(void*)>a.class_instance_size(dataClass)) {
        *error="MountData.delegations outside class layout";return false;
    }
    auto fetch=[&](Il2CppObject* object) {
        Il2CppObject* value=nullptr;
        std::memcpy(&value,reinterpret_cast<char*>(object)+offset,sizeof(value));
        return value;
    };
    Il2CppObject* owned=fetch(custom),*meowDelegates=fetch(items[meow]),*fartDelegates=fetch(items[fart]);
    if(!meowDelegates||!fartDelegates)return *error="Vanilla minecart delegates null",false;
    if(!owned) {
        owned=a.object_new(delegateClass);
        if(!owned||!setObject(custom,dataClass,"delegations",owned,error))return false;
    }
    struct Ref {const char* name;Il2CppObject* source;};
    for(const auto& ref:std::array<Ref,3>{
        Ref{"MinecartDust",meowDelegates},
        Ref{"MinecartLandingSound",fartDelegates},
        Ref{"MinecartBumperSound",fartDelegates}}) {
        auto* member=findField(delegateClass,ref.name);
        if(!member||(a.field_get_flags(member)&0x10u)) {
            *error=std::string("Vanilla minecart field missing: ")+ref.name;return false;
        }
        const size_t pos=a.field_get_offset(member);
        if(pos<sizeof(Il2CppObject)||pos+sizeof(void*)>a.class_instance_size(delegateClass)) {
            *error="Minecart delegate offset invalid";return false;
        }
        Il2CppObject* handler=nullptr;
        std::memcpy(&handler,reinterpret_cast<char*>(ref.source)+pos,sizeof(handler));
        if(!handler){*error=std::string("Vanilla minecart handler null: ")+ref.name;return false;}
        auto** cell=reinterpret_cast<void**>(reinterpret_cast<char*>(owned)+pos);
        a.gc_wbarrier_set_field(owned,cell,handler);
    }
    return true;
}
bool writeFields(Il2CppObject* object,const mount_policy::Definition& values,
                 Il2CppObject* back,Il2CppObject* front,int width,int height,
                 int buff,std::string* error) {
    const auto& d=values;
    if(!setInt(object,dataClass,"jumpHeight",d.jumpHeight,error)||
       !setFloat(object,dataClass,"acceleration",d.acceleration,error)||
       !setFloat(object,dataClass,"jumpSpeed",d.jumpSpeed,error)||
       !setBool(object,dataClass,"blockExtraJumps",d.blockExtraJumps,error)||
       !setBool(object,dataClass,"constantJump",d.constantJump,error)||
       !setInt(object,dataClass,"heightBoost",d.heightBoost,error)||
       !setFloat(object,dataClass,"fallDamage",d.fallDamage,error)||
       !setFloat(object,dataClass,"runSpeed",d.runSpeed,error)||
       !setFloat(object,dataClass,"dashSpeed",d.dashSpeed,error)||
       !setInt(object,dataClass,"flightTimeMax",d.flightTime,error)||
       !setInt(object,dataClass,"fatigueMax",d.fatigue,error)||
       !setInt(object,dataClass,"buff",buff,error)||
       !setInt(object,dataClass,"spawnDust",d.dust=="21"?21:91,error)||
       !setInt(object,dataClass,"totalFrames",d.totalFrames,error)||
       !setInt(object,dataClass,"xOffset",d.xOffset,error)||
       !setInt(object,dataClass,"yOffset",d.yOffset,error)||
       !setInt(object,dataClass,"playerHeadOffset",d.headOffset,error)||
       !setInt(object,dataClass,"bodyFrame",d.bodyFrame,error)||
       !setInt(object,dataClass,"textureWidth",width,error)||
       !setInt(object,dataClass,"textureHeight",height,error)||
       !setInt(object,dataClass,"standingFrameCount",d.frames[0].count,error)||
       !setInt(object,dataClass,"standingFrameDelay",d.frames[0].delay,error)||
       !setInt(object,dataClass,"standingFrameStart",d.frames[0].start,error)||
       !setInt(object,dataClass,"runningFrameCount",d.frames[1].count,error)||
       !setInt(object,dataClass,"runningFrameDelay",d.frames[1].delay,error)||
       !setInt(object,dataClass,"runningFrameStart",d.frames[1].start,error)||
       !setInt(object,dataClass,"flyingFrameCount",d.frames[2].count,error)||
       !setInt(object,dataClass,"flyingFrameDelay",d.frames[2].delay,error)||
       !setInt(object,dataClass,"flyingFrameStart",d.frames[2].start,error)||
       !setInt(object,dataClass,"inAirFrameCount",d.frames[3].count,error)||
       !setInt(object,dataClass,"inAirFrameDelay",d.frames[3].delay,error)||
       !setInt(object,dataClass,"inAirFrameStart",d.frames[3].start,error)||
       !setInt(object,dataClass,"idleFrameCount",d.frames[4].count,error)||
       !setInt(object,dataClass,"idleFrameDelay",d.frames[4].delay,error)||
       !setInt(object,dataClass,"idleFrameStart",d.frames[4].start,error)||
       !setBool(object,dataClass,"idleFrameLoop",d.idleLoop,error)||
       !setInt(object,dataClass,"swimFrameCount",d.frames[5].count,error)||
       !setInt(object,dataClass,"swimFrameDelay",d.frames[5].delay,error)||
       !setInt(object,dataClass,"swimFrameStart",d.frames[5].start,error))return false;
    auto* ints=findClass({"System","Int32",{}});
    auto* offsets=api().array_new(ints,static_cast<uintptr_t>(d.totalFrames));
    if(!offsets)return false;
    auto* offsetData=static_cast<int32_t*>(arrayData(offsets));
    for(int frame=0;frame<d.totalFrames;++frame)offsetData[frame]=d.playerYOffset;
    if(!setObject(object,dataClass,"playerYOffsets",
                  reinterpret_cast<Il2CppObject*>(offsets),error))return false;
    if(back&&!setObject(object,dataClass,"backTexture",back,error))return false;
    if(front&&!setObject(object,dataClass,"frontTexture",front,error))return false;
    return true;
}
}

int nativeMountType(const std::string& mod,const std::string& name) {
    auto found=keys.find(keyOf(mod,name));
    return found==keys.end()?-1:found->second;
}
int reserveNativeMount(const std::string& mod,const std::string& name,std::string* error) {
    if(!error)return -1;
    if(mod.empty()||name.empty()||name.size()>72||keys.count(keyOf(mod,name))) {
        *error="Invalid or duplicate mount identity";return -1;
    }
    if(!preflight(error))return -1;
    int64_t count=0;
    if(!native_static::readInteger(countValue,&count)||count<=0||count>=2048) {
        *error="MountID.Count outside native reservation bounds";return -1;
    }
    const int type=static_cast<int>(count);
    if(!native_static::writeInteger(countValue,count+1)) {
        *error="MountID.Count write failed";return -1;
    }
    slots.emplace(type,Slot{mod,name,type,-1,false});
    keys.emplace(keyOf(mod,name),type);
    return type;
}
bool publishNativeMounts(const std::string& mod,const std::string& directory,
                         const std::vector<ManagedMountEntry>& entries,std::string* error) {
    if(!error)return false;
    if(entries.empty())return true;
    if(!preflight(error,true))return false;
    for(const auto& entry:entries) {
        if(entry.type<0||!slotArray(entry.type,error))return false;
    }
    if(!hookMountEffects(error))return false;
    struct Pending {
        int type=0,buff=0;bool minecart=false;
        Il2CppObject* data=nullptr;
        std::vector<std::shared_ptr<AssetRoot>> textures;
    };
    std::vector<Pending> pending;
    pending.reserve(entries.size());
    for(const auto& entry:entries) {
        auto slot=slots.find(entry.type);
        if(slot==slots.end()||slot->second.mod!=mod||slot->second.name!=entry.name||
           slot->second.published||entry.values.texture.empty()) {
            *error="Mount reservation, identity or revision mismatch: "+entry.name;return false;
        }
        const int buff=runtime::modBuffTypeByName(mod,entry.values.buff);
        if(buff<0) {*error="Mount requires a registered local buff: "+entry.values.buff;return false;}
        const std::string stem=directory+"/content/"+entry.values.texture;
        const bool cart=entry.values.kind==1;
        const std::string front=stem+"_Front.png",back=stem+"_Back.png";
        const std::string balloon=directory+"/content/"+entry.values.balloon+".png";
        if(!cart&&!file(balloon)) {
            *error="Missing original mount balloon texture: "+entry.name;return false;
        }
        if(!file(front)||(!cart&&!file(back))) {
            *error="Missing original mount front/back texture: "+entry.name;return false;
        }
        Pending candidate;candidate.type=entry.type;candidate.buff=buff;candidate.minecart=cart;
        int width=0,height=0;
        Il2CppObject* backAsset=nullptr;
        if(!cart) {
            backAsset=runtime::content::loadTextureAsset(back,nullptr,0,
                "CSharpMount:"+std::to_string(entry.type)+":Back",&width,&height);
            if(!backAsset){*error="Mount back texture upload failed";return false;}
            candidate.textures.push_back(std::make_shared<AssetRoot>(backAsset));
        }
        int frontWidth=0,frontHeight=0;
        auto* frontAsset=runtime::content::loadTextureAsset(front,nullptr,0,
            "CSharpMount:"+std::to_string(entry.type)+":Front",&frontWidth,&frontHeight);
        if(!frontAsset){*error="Mount front texture upload failed";return false;}
        candidate.textures.push_back(std::make_shared<AssetRoot>(frontAsset));
        if(!cart) {
            int balloonW=0,balloonH=0;
            auto* balloonAsset=runtime::content::loadTextureAsset(balloon,nullptr,0,
                "CSharpMount:"+std::to_string(entry.type)+":Balloons",&balloonW,&balloonH);
            if(!balloonAsset||balloonW!=56||balloonH!=224) {
                *error="Original mount balloon sheet must be 56x224";return false;
            }
            candidate.textures.push_back(std::make_shared<AssetRoot>(balloonAsset));
        }
        if(cart){width=frontWidth;height=frontHeight;}
        else width+=20;
        auto& a=api();
        auto* object=a.object_new(dataClass);
        const auto* ctor=a.class_get_method_from_name(dataClass,".ctor",0);
        if(!object||!ctor||!a.method_is_instance(ctor)) {
            *error="MountData constructor unavailable";return false;
        }
        Il2CppObject* exception=nullptr;
        a.runtime_invoke(ctor,object,nullptr,&exception);
        if(exception){*error="MountData constructor rejected";return false;}
        if(cart) {
            auto* cls=findClass({"Terraria","Mount",{}});
            const auto* set=a.class_get_method_from_name(cls,"SetAsMinecart",5);
            if(!set||a.method_is_instance(set)) {
                *error="Mount.SetAsMinecart unavailable";return false;
            }
            const int32_t vertical=0;
            void* args[]={object,&candidate.buff,frontAsset,const_cast<int32_t*>(&vertical),
                          const_cast<int32_t*>(&vertical)};
            a.runtime_invoke(set,nullptr,args,&exception);
            if(exception){*error="Mount.SetAsMinecart failed";return false;}
            if(!copyMinecartDelegates(object,error))return false;
        } else if(!writeFields(object,entry.values,backAsset,frontAsset,
                            width,height,buff,error))return false;
        if(cart&&!setInt(object,dataClass,"spawnDust",21,error))return false;
        candidate.data=object;
        pending.push_back(std::move(candidate));
    }
    for(auto& candidate:pending) {
        if(!setMountData(candidate.type,candidate.data,error)||
           !writeCart(candidate.type,candidate.minecart,error)||
           !writeMountBuff(candidate.buff,candidate.type,error))return false;
        assets[candidate.type]=std::move(candidate.textures);
        auto& slot=slots[candidate.type];
        slot.published=true;slot.buff=candidate.buff;
        if(!candidate.minecart)requestNativeVisualRenderer("");
        const auto policy=std::find_if(entries.begin(),entries.end(),
            [&](const ManagedMountEntry& entry){return entry.type==candidate.type;});
        if(policy!=entries.end())slot.policy=policy->values;
    }
    BL_INFO("[CSharp][Mount] Mod=%s Count=%zu Stage=Publish Result=Ready",
            mod.c_str(),pending.size());
    return true;
}
void updateNativeMountBuffs(Il2CppObject* player) {
    if(!player||!effectsHooked||slots.empty()||playerMountOffset<0||
       playerBuffTypeOffset<0||playerBuffTimeOffset<0||!setMountMethod)return;
    auto* bytes=reinterpret_cast<char*>(player);
    Il2CppArray* types=nullptr,*times=nullptr;
    std::memcpy(&types,bytes+playerBuffTypeOffset,sizeof(types));
    std::memcpy(&times,bytes+playerBuffTimeOffset,sizeof(times));
    if(!nativeArrayContract(types,"Int32",4,60)||
       !nativeArrayContract(times,"Int32",4,60))return;
    const auto count=std::min(types->length,times->length);
    auto* ids=static_cast<int32_t*>(arrayData(types));
    auto* durations=static_cast<int32_t*>(arrayData(times));
    Il2CppObject* mount=nullptr;
    std::memcpy(&mount,bytes+playerMountOffset,sizeof(mount));
    if(!mount)return;
    for(uintptr_t i=0;i<count;++i) {
        const int buff=ids[i];
        if(buff<=0)continue;
        for(const auto& [type,entry]:slots) {
            if(!entry.published||entry.policy.kind!=0||entry.buff!=buff)continue;
            durations[i]=10;
            if(activeMountType(mount)!=type) {
                int nativeType=type;uint8_t ignore=0;
                void* args[]={&nativeType,player,&ignore};
                Il2CppObject* exception=nullptr;
                api().runtime_invoke(setMountMethod,mount,args,&exception);
                if(exception){static unsigned faults=0;if(faults++<3)
                    BL_WARN("[CSharp][Mount] Stage=RefreshBuff Result=Exception");}
            }
            return;
        }
    }
}
bool nativeMountBalloonState(Il2CppObject* player,std::array<float,3>* rotations,
                            Il2CppObject** texture) {
    if(!player||!rotations||!texture||playerMountOffset<0||!effectsHooked)return false;
    const int index=playerIndex(player);
    if(index<0)return false;
    Il2CppObject* mount=nullptr;
    std::memcpy(&mount,reinterpret_cast<char*>(player)+playerMountOffset,sizeof(mount));
    const int type=activeMountType(mount);
    const auto it=slots.find(type);
    if(it==slots.end()||!it->second.published||it->second.policy.kind!=0)return false;
    auto asset=assets.find(type);
    if(asset==assets.end()||asset->second.size()!=3||!asset->second[2])return false;
    auto* textureAsset=asset->second[2]->get();
    auto* value=textureAsset?runtime::content::textureAssetValue(textureAsset):nullptr;
    if(!value)return false;
    const auto& state=balloons[static_cast<size_t>(index)];
    *rotations=state.mount==type?state.rotations:std::array<float,3>{};
    *texture=value;
    return true;
}
void retireNativeMounts(const std::string& mod) {
    std::string error;
    for(auto i=slots.begin();i!=slots.end();) {
        if(i->second.mod!=mod){++i;continue;}
        if(i->second.published) {
            setMountData(i->first,nullptr,&error);
            if(i->second.buff>=0)writeMountBuff(i->second.buff,-1,&error);
            writeCart(i->first,false,&error);
        }
        assets.erase(i->first);
        keys.erase(keyOf(mod,i->second.name));
        i=slots.erase(i);
    }
}
}
