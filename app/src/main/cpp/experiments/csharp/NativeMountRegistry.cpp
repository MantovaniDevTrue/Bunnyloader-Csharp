#include "experiments/csharp/NativeMountRegistry.h"
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
std::string keyOf(const std::string& mod,const std::string& name) {return mod+"/"+name;}
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
bool preflight(std::string* error) {
    auto& a=api();
    if(!a.runtime_invoke||!a.object_new||!a.array_new||!a.field_static_get_value||
       !a.field_static_set_value||!a.class_get_element_class||
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
    Il2CppArray* mounted=nullptr;
    if(!native_static::read(mountsValue,&mounted,sizeof(mounted))||!mounted||
       mounted->length<static_cast<uintptr_t>(n)||mounted->length>4096||
       a.class_get_element_class(mounted->obj.klass)!=dataClass) {
        *error="Mount.mounts requires MountData[] with safe capacity";return false;
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
    if(!slotArray(type,error))return -1;
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
    if(!preflight(error))return false;
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
    }
    BL_INFO("[CSharp][Mount] Mod=%s Count=%zu Stage=Publish Result=Ready",
            mod.c_str(),pending.size());
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
