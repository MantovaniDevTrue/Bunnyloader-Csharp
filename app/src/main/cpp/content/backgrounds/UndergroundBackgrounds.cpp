#include "content/backgrounds/UndergroundBackgrounds.h"
#include "content/backgrounds/BackgroundAssetLifetime.h"
#include "content/common/ContentAssets.h"
#include "il2cpp/CheckedProperty.h"
#include "il2cpp/ValueLayout.h"
#include "il2cpp/Signature.h"
#include <algorithm>
#include "hook/HookManager.h"
#include "boot/Boot.h"
#include "core/Log.h"
#include <mutex>
#include <map>
#include <vector>
#include <cmath>
#include <atomic>
#include <unistd.h>

namespace bl::runtime::underground {
namespace {
using Property=il2cpp::CheckedProperty;
struct Entry {
    std::string mod,name;Language language;bool active=false,retired=false;
    int priority=0;double weight=.5;uint64_t frame=0;bool unavailable=false;
    std::array<int,5> leases{};
    Slots slots{-1,-1,-1,-1,6};std::array<uint32_t,5> roots{};
};
struct Provider { Choose choose=nullptr;Fill fill=nullptr; };
std::recursive_mutex mutex;
std::map<int,Entry> entries;
std::map<std::pair<std::string,std::string>,int> keys;
int next=22;
uint64_t frame=0;
Provider providers[2];
struct Refs {
    Property menu,enabled,position,width,caveX,caveStyle,current,old,assets,local,back,oldBack,values,oldValues;
    size_t xOffset=0;
    const MethodInfo* picker=nullptr;
    bool ready=false;
} refs;
using Picker=int(*)(double,const MethodInfo*);
using Getter=Il2CppArray*(*)(void*,const MethodInfo*);
Picker originalPick=nullptr;Getter originalValues=nullptr,originalOldValues=nullptr;
bool failure(std::string* error,const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    if(error)*error="[Native][ERROR][UndergroundBackground] Stage="+std::string(stage)+" Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}
void diagnostic(const std::string& error) {
    static std::atomic<unsigned> count{0};const unsigned n=count.fetch_add(1)+1;
    if(n && !(n&(n-1)))BL_ERROR("%s SuppressedTotal=%u",error.c_str(),n-1);
}
bool identity(const std::string& text) {
    if(text.empty() || text.size()>240)return false;
    for(unsigned char c:text)if(c<32 || c==127 || c=='/' || c=='\\' || c=='|')return false;
    return true;
}
void release(Entry& entry) {
    for(auto& root:entry.roots)if(root){il2cpp::api().gchandle_free(root);root=0;}
    for(auto& lease:entry.leases)if(lease){backgroundAssets::release(lease);lease=0;}
}
void assetRetiring(int slot) {
    std::lock_guard lock(mutex);
    for(auto& [id,own]:entries){(void)id;for(int lease:own.leases)if(lease==slot){own.unavailable=true;break;}}
}
bool bind(std::string* error) {
    if(refs.ready)return true;
    auto& a=il2cpp::api();
    if(!a.gchandle_new || !a.gchandle_free || !a.gchandle_get_target || !a.gc_wbarrier_set_field)
        return failure(error,"Bind","MetadataUnavailable","GcRootsAndWriteBarrier","Missing");
    auto* main=il2cpp::findClassQuiet("Terraria","Main");
    auto* local=il2cpp::findClassQuiet("","LocalUserGameState");
    auto* texture=il2cpp::findClassQuiet("Microsoft.Xna.Framework.Graphics","Texture2D");
    auto* assets=il2cpp::findClassQuiet("Terraria.GameContent","TextureAssets");
    if(!main || !local || !texture || !assets)
        return failure(error,"BindClasses","NativeClassMissing","Terraria.Main,LocalUserGameState,Texture2D,TextureAssets",
            std::string(!main?"Missing=Terraria.Main ":"")+(!local?"Missing=LocalUserGameState ":"")+(!texture?"Missing=Microsoft.Xna.Framework.Graphics.Texture2D ":"")+(!assets?"Missing=Terraria.GameContent.TextureAssets":""));
    Refs staged;
    using K=Property::Kind;
    if(!staged.menu.bind(main,"gameMenu",K::Boolean,true,error) || !staged.enabled.bind(main,"BackgroundEnabled",K::Boolean,true,error) ||
       !staged.position.bind(main,"screenPosition",K::Vector2,true,error) || !staged.width.bind(main,"screenWidth",K::Int32,true,error) ||
       !staged.caveX.bind(main,"caveBackX",K::IntArray,true,error) || !staged.caveStyle.bind(main,"caveBackStyle",K::IntArray,true,error) ||
       !staged.current.bind(main,"undergroundBackground",K::Int32,true,error) || !staged.old.bind(main,"oldUndergroundBackground",K::Int32,true,error) ||
       !staged.assets.bind(assets,"Background",K::AssetArray,true,error,content::textureAssetClass()) ||
       !staged.local.bind(local,"Instance",K::Reference,true,error,local) ||
       !staged.back.bind(local,"backTexture",K::IntArray,false,error) || !staged.oldBack.bind(local,"oldBackTexture",K::IntArray,false,error) ||
       !staged.values.bind(main,"backTextureValues",K::TextureArray,false,error,texture,true) ||
       !staged.oldValues.bind(main,"oldBackTextureValues",K::TextureArray,false,error,texture,true))return false;
    il2cpp::ValueLayout layout;
    if(!il2cpp::resolveValueLayout(staged.position.declared,&layout,error))return false;
    auto* x=il2cpp::findField(staged.position.declared,"X");
    if(!x || (a.field_get_flags(x)&0x10u) || !layout.offset(a.field_get_offset(x),4,&staged.xOffset))return failure(error,"Bind","NativeVectorLayoutMismatch","Vector2.X:InstanceSingleWithin8Bytes","MissingStaticOrOutOfRange");
    auto* xType=a.class_from_il2cpp_type(a.field_get_type(x));uint32_t align=0;
    if(Property::name(xType)!="System.Single" || !a.class_is_valuetype(xType) || a.class_value_size(xType,&align)!=4)
        return failure(error,"Bind","NativeVectorLayoutMismatch","Vector2.X:System.Single:4",Property::name(xType));
    const auto* method=il2cpp::findMethodBySignature(main,il2cpp::parseSignature("int DrawBackground_PickUndergroundBackgroundStyle(double magmaLayer)"));
    if(!method || !a.method_get_flags || !(a.method_get_flags(method,nullptr)&0x10u) || a.method_get_param_count(method)!=1 ||
       Property::name(a.class_from_il2cpp_type(a.method_get_return_type(method)))!="System.Int32" ||
       Property::name(a.class_from_il2cpp_type(a.method_get_param(method,0)))!="System.Double" ||
       !il2cpp::methodPointer(method) || (a.method_is_generic && a.method_is_generic(method)))
        return failure(error,"BindPicker","NativeMethodContractMismatch","Static:Int32(Double)","MissingOrWrongSignature");
    if(!il2cpp::methodPointer(staged.values.getter) || !il2cpp::methodPointer(staged.oldValues.getter))
        return failure(error,"BindTextureGetters","NativeMethodBodyMissing","CurrentAndPreviousTextureGetterBodies","MissingNativeBody");
    staged.picker=method;staged.ready=true;refs=staged;return true;
}
Entry* entry(int slot) { auto it=entries.find(slot);return it==entries.end()?nullptr:&it->second; }
bool prepare(Entry& own,int slot,std::unique_lock<std::recursive_mutex>& lock,std::string* error) {
    if(own.frame==frame)return true;
    const uint64_t serial=frame;own.frame=serial;
    auto fill=providers[int(own.language)].fill;
    Slots slots{-1,-1,-1,-1,6};
    if(!fill)return failure(error,"FillTextureArray","ProviderUnavailable","InstalledLanguageProvider",own.mod+"/"+own.name);
    lock.unlock();const bool filled=fill(slot,&slots,error);lock.lock();
    if(!filled) {
        if(error->empty())failure(error,"FillTextureArray","CallbackRejected","FiveTextureSlots",own.mod+"/"+own.name);
        return false;
    }
    if(serial!=frame || own.retired)return failure(error,"FillTextureArray","StaleWorldOrRetiredStyle","CurrentDrawSession",std::to_string(serial));
    Il2CppArray* assets=nullptr;
    if(!refs.assets.read(nullptr,&assets,error) || !refs.assets.array(assets,1,error))return false;
    std::array<uint32_t,5> roots{};std::array<int,5> leases{};
    auto& a=il2cpp::api();bool changed=false;
    for(size_t i=0;i<5;++i) {
        if(own.language==Language::Managed && (slots[i]<-1 || (slots[i]>=0 && size_t(slots[i])>=assets->length))) {
            for(size_t k=0;k<i;++k)if(roots[k] && roots[k]!=own.roots[k]){a.gchandle_free(roots[k]);if(leases[k])backgroundAssets::release(leases[k]);}
            return failure(error,"ResolveTextureSlot","InvalidTextureSlot","TextureSlots["+std::to_string(i)+"]:-1.."+std::to_string(assets->length-1),std::to_string(slots[i]));
        }
        if(slots[i]>=0 && backgroundAssets::retired(slots[i])) {
            for(size_t k=0;k<i;++k)if(roots[k] && roots[k]!=own.roots[k]){a.gchandle_free(roots[k]);if(leases[k])backgroundAssets::release(leases[k]);}
            return failure(error,"ResolveTextureSlot","AssetOwnerUnloaded","LiveAssetForTextureSlots["+std::to_string(i)+"]",std::to_string(slots[i]));
        }
        // Missing/invalid slots preserve the game's original layer, as in JS.
        Il2CppObject* image=nullptr;
        if(slots[i]>=0 && size_t(slots[i])<assets->length) {
            auto* asset=static_cast<Il2CppObject**>(arrayData(assets))[slots[i]];
            if(asset)image=content::textureAssetValue(asset);
            if(image && !a.class_is_assignable_from(a.class_get_element_class(refs.values.declared),a.object_get_class(image))) {
                for(size_t k=0;k<i;++k)if(roots[k] && roots[k]!=own.roots[k]){a.gchandle_free(roots[k]);if(leases[k])backgroundAssets::release(leases[k]);}
                return failure(error,"ResolveTexture","NativeTextureTypeMismatch","Texture2D",Property::name(a.object_get_class(image)));
            }
        }
        if(!image && slots[i]>=0 && own.language==Language::Managed) {
            for(size_t k=0;k<i;++k)if(roots[k] && roots[k]!=own.roots[k]){a.gchandle_free(roots[k]);if(leases[k])backgroundAssets::release(leases[k]);}
            return failure(error,"ResolveTextureSlot","AssetNotReady","LoadedTexture2DForSlot",std::to_string(slots[i]));
        }
        auto* old=own.roots[i]?a.gchandle_get_target(own.roots[i]):nullptr;
        if(image==old && slots[i]==own.slots[i]){roots[i]=own.roots[i];leases[i]=own.leases[i];}
        else if(image) {
            roots[i]=a.gchandle_new(image,false);
            if(roots[i])leases[i]=backgroundAssets::retain(slots[i]);
            if(!roots[i]) {
                for(size_t k=0;k<i;++k)if(roots[k] && roots[k]!=own.roots[k]){a.gchandle_free(roots[k]);if(leases[k])backgroundAssets::release(leases[k]);}
                return failure(error,"RetainTexture","GcRootAllocationFailed","RootedFiveLayerSnapshot",std::to_string(i));
            }
        }
        changed|=roots[i]!=own.roots[i];
    }
    if(changed)for(size_t i=0;i<5;++i)if(own.roots[i] && own.roots[i]!=roots[i]){a.gchandle_free(own.roots[i]);if(own.leases[i])backgroundAssets::release(own.leases[i]);}
    own.roots=roots;own.leases=leases;own.slots=slots;own.frame=serial;own.unavailable=false;
    return true;
}
bool hasTexture(const Entry& own) {for(auto root:own.roots)if(root)return true;return false;}
int choose(int vanilla,double magma) {
    (void)magma;
    std::unique_lock lock(mutex);const uint64_t serial=++frame;
    std::string error;
    bool menu=true,enabled=false;
    if(!refs.menu.read(nullptr,&menu,&error) || !refs.enabled.read(nullptr,&enabled,&error)){diagnostic(error);return vanilla;}
    if(menu || !enabled)return vanilla;
    Candidate best;const Entry* bestOwner=nullptr;
    const auto offer=[&](Candidate candidate) {
        auto* own=entry(candidate.slot);
        if(!own || own->retired || candidate.priority<0 || candidate.priority>8 || !std::isfinite(candidate.weight))return;
        candidate.weight=std::clamp(candidate.weight,0.0,1.0);
        const double rank=candidate.priority+candidate.weight, oldRank=best.priority+best.weight;
        const bool earlier=bestOwner && (own->mod<bestOwner->mod || (own->mod==bestOwner->mod && own->name<bestOwner->name));
        if(best.slot<0 || rank>oldRank || (rank==oldRank && earlier)){best=candidate;bestOwner=own;}
    };
    for(auto& [slot,own]:entries)if(own.active && !own.retired)offer({slot,own.priority,own.weight});
    for(const auto& provider:providers)if(provider.choose) {
        const auto callback=provider.choose;lock.unlock();const auto candidate=callback();lock.lock();if(serial!=frame)return vanilla;offer(candidate);
    }
    // Selection and Fill are distinct: getters request only layers the engine
    // really uses. This preserves the original once-per-draw callback behavior.
    int current=-1,old=-1;
    if(!refs.current.read(nullptr,&current,&error) || !refs.old.read(nullptr,&old,&error)){diagnostic(error);return vanilla;}
    for(auto& [slot,own]:entries)if((own.retired || own.unavailable) && slot!=current && slot!=old)release(own);
    if(serial!=frame || best.slot<0 || entry(best.slot)->retired)return vanilla;
    auto* selected=entry(best.slot);
    // A missing owner must stop selection; only an explicit successful fill
    // can recover it when that dependency is loaded again.
    if(selected->unavailable) {
        if(!prepare(*selected,best.slot,lock,&error))diagnostic(error+" Mod="+selected->mod+" Class="+selected->name+" Consequence=VanillaFallback");
        if(selected->unavailable || serial!=frame)return vanilla;
    }
    if(best.priority>=3)return best.slot;
    Il2CppArray *xs=nullptr,*styles=nullptr;int width=0;std::array<unsigned char,8> position{};float x=0;
    if(!refs.position.read(nullptr,position.data(),&error) || !refs.width.read(nullptr,&width,&error) ||
       !refs.caveX.read(nullptr,&xs,&error) || !refs.caveStyle.read(nullptr,&styles,&error) ||
       !refs.caveX.array(xs,3,&error) || !refs.caveStyle.array(styles,4,&error)){diagnostic(error);return vanilla;}
    std::memcpy(&x,position.data()+refs.xOffset,4);
    if(!std::isfinite(x))return vanilla;
    const double tile=std::floor((double(x)+std::floor(double(width)/2))/16);
    const auto* limits=static_cast<const int32_t*>(arrayData(xs));const auto* values=static_cast<const int32_t*>(arrayData(styles));
    const int64_t cave=int64_t(values[tile>limits[0]?(tile>limits[1]?(tile>limits[2]?3:2):1):0])+3;
    return (best.priority>=1 && vanilla==cave) || (best.priority>=2 && (vanilla==1 || vanilla==11))?best.slot:vanilla;
}
int hkPick(double magma,const MethodInfo* method) {
    const int vanilla=originalPick(magma,method);
    // Android can request background selection from a drawing thread. The
    // shared service only reads checked metadata and publishes bounded mail.
    if(gameThreadId()<=0)return vanilla;
    return choose(vanilla,magma);
}
Il2CppArray* patch(void* self,Il2CppArray* values,bool old) {
    std::unique_lock lock(mutex);std::string error;int style=-1;
    const uint64_t serial=frame;
    if(!(old?refs.old:refs.current).read(nullptr,&style,&error)){diagnostic(error);return values;}
    auto* own=entry(style);if(!own)return values;
    bool menu=true,enabled=false;
    if(!refs.menu.read(nullptr,&menu,&error) || !refs.enabled.read(nullptr,&enabled,&error)){diagnostic(error);return values;}
    if(menu || !enabled)return values;
    if(!own->retired && own->frame!=frame && !prepare(*own,style,lock,&error))
        diagnostic(error+" Mod="+own->mod+" Class="+own->name+" ContentId="+std::to_string(style)+
            (hasTexture(*own)?" Consequence=UsingLastValidFill":" Consequence=OriginalGetterFallback"));
    if(serial!=frame || !hasTexture(*own))return values;
    Il2CppObject* local=nullptr;Il2CppArray* numbers=nullptr;
    const auto& numberProperty=old?refs.oldBack:refs.back;
    if(!refs.local.read(nullptr,&local,&error) || !numberProperty.read(local,&numbers,&error) ||
       !numberProperty.array(numbers,5,&error) || !(old?refs.oldValues:refs.values).array(values,5,&error)){diagnostic(error);return values;}
    auto& a=il2cpp::api();auto** textures=static_cast<Il2CppObject**>(arrayData(values));auto* ids=static_cast<int32_t*>(arrayData(numbers));
    for(size_t i=0;i<5;++i)if(own->roots[i]) {
        auto* texture=a.gchandle_get_target(own->roots[i]);
        if(texture && (textures[i]!=texture || ids[i]!=own->slots[i])) {
            a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(values),reinterpret_cast<void**>(textures+i),texture);
            ids[i]=own->slots[i];
        }
    }
    (void)self;return values;
}
Il2CppArray* hkValues(void* self,const MethodInfo* method){return patch(self,originalValues(self,method),false);}
Il2CppArray* hkOldValues(void* self,const MethodInfo* method){return patch(self,originalOldValues(self,method),true);}
}
int reserve(const std::string& mod,const std::string& name,Language language,std::string* error) {
    std::lock_guard lock(mutex);
    if(!identity(mod) || mod.size()>128 || !identity(name))return failure(error,"Reserve","InvalidIdentity","ModUidAndClassName",mod+"/"+name),-1;
    const auto key=std::make_pair(mod,name);auto found=keys.find(key);
    if(found!=keys.end()) {
        auto& own=entries.at(found->second);
        if(own.language!=language)return failure(error,"Reserve","LanguageIdentityCollision","OneOwnerPerUidAndClass",mod+"/"+name),-1;
        own.retired=false;return found->second;
    }
    if(entries.size()>=4096)return failure(error,"Reserve","CapacityExceeded","Styles<=4096",std::to_string(entries.size())),-1;
    const int slot=next++;entries.emplace(slot,Entry{mod,name,language});keys.emplace(key,slot);return slot;
}
int find(const std::string& mod,const std::string& name){std::lock_guard lock(mutex);auto it=keys.find({mod,name});return it==keys.end() || entries.at(it->second).retired?-1:it->second;}
bool select(int slot,int priority,double weight,bool active,std::string* error) {
    std::lock_guard lock(mutex);auto* own=entry(slot);
    if(!own || own->retired || priority<0 || priority>8 || !std::isfinite(weight) || weight<0 || weight>1)
        return failure(error,"Select","InvalidSelection","RegisteredSlot,Priority0..8,Weight0..1",std::to_string(slot));
    own->active=active;own->priority=priority;own->weight=weight;return true;
}
void retireLanguage(Language language){std::lock_guard lock(mutex);for(auto& [slot,own]:entries){(void)slot;if(own.language==language){own.active=false;own.retired=true;}}}
void retire(int slot){std::lock_guard lock(mutex);if(auto* own=entry(slot)){own->active=false;own->retired=true;}}
int count(){std::lock_guard lock(mutex);return int(entries.size());}
void provider(Language language,Choose choose,Fill fill){std::lock_guard lock(mutex);providers[int(language)]={choose,fill};}
bool install(std::string* error) {
    std::lock_guard lock(mutex);
    if(originalPick && originalValues && originalOldValues)return true;
    if(!bind(error))return false;
    if(!backgroundAssets::observeRetirement(assetRetiring))return failure(error,"Install","RetirementObserverCapacityExceeded","AvailableRendererObserver","Full");
    if(!originalValues && !hook::install(refs.values.getter,hkValues,&originalValues))return failure(error,"Install","HookRejected","CurrentTextureGetter","Rejected");
    if(!originalOldValues && !hook::install(refs.oldValues.getter,hkOldValues,&originalOldValues))return failure(error,"Install","HookRejected","PreviousTextureGetter","Rejected");
    if(!originalPick && !hook::install(refs.picker,hkPick,&originalPick))return failure(error,"Install","HookRejected","StylePicker","Rejected");
    return true;
}
void resetWorld(){std::lock_guard lock(mutex);++frame;for(auto& [slot,own]:entries){(void)slot;own.active=false;own.frame=0;}}
}
