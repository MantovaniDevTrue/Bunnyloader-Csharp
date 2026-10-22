#include "content/backgrounds/SurfaceBackgrounds.h"
#include "content/backgrounds/SurfaceProperties.h"
#include "content/backgrounds/SurfaceTables.h"
#include "content/backgrounds/SurfaceSpriteBatch.h"
#include "content/backgrounds/BackgroundAssetLifetime.h"
#include "content/common/ContentAssets.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>

namespace bl::runtime::surface {
namespace {
using P=il2cpp::CheckedProperty;using K=P::Kind;
struct Layer {int slot=-1,lease=0;uint32_t root=0;TextureSize size;CloseSettings close;bool defaultClose=true;};
struct Entry {
    std::string mod,name;Language language;bool active=false,menu=false,retired=false,unavailable=false;
    int priority=0,inflight=0;double weight=.5;std::array<Layer,3> layers{};
};
struct Provider {Choose choose=nullptr;Menu menu=nullptr;Invoke invoke=nullptr;};
struct Table {
    il2cpp::CheckedFieldWrite field;bool local=false;
    bool bind(Il2CppClass* main,Il2CppClass* localClass,const char* name,std::string* error) {
        if(il2cpp::findField(main,name))return field.bind(main,name,K::SingleArray,true,error);
        P visible;if(!visible.bind(main,name,K::SingleArray,true,error))return false;
        local=true;
        return field.bind(localClass,name,K::SingleArray,false,error) &&
            (visible.declared==field.contract().declared || visible.reject(error,"BindBackingField",P::name(visible.declared),P::name(field.contract().declared),"NativeBackingTypeMismatch"));
    }
};
struct Refs {
    P menu,local,instance,baseColor,batch,sky,assets,widths,heights,position,screenWidth;
    NumberProperty styleRead,speed,loops,start,top,step,scale,adjust,surface;
    PropertyWrite style,modified;
    Table front,far;il2cpp::CheckedFieldWrite forest,desert;
    il2cpp::NullableLayout nullableInt,nullableFloat;
    csharp::NativeColorLayout color;
    SurfaceSpriteBatch renderer;Il2CppClass* texture=nullptr;
    size_t x=0,y=0;int vanilla=0;
    const MethodInfo *preferred=nullptr,*back=nullptr,*frontMethod=nullptr,*step1=nullptr,*step2=nullptr,*fog=nullptr,*modify=nullptr;
    bool ready=false;
} refs;
std::recursive_mutex mutex;
std::mutex drawTransaction;
std::map<int,Entry> entries;
std::map<std::pair<std::string,std::string>,int> keys;
Provider providers[2];int next=-1;
std::atomic<bool> awake{false},hooksReady{false};
std::atomic<uint64_t> epoch{1};
struct Step {double top=0;int push=0;uint64_t epoch=0;bool valid=false;} step1State;
using Preferred=int(*)(const MethodInfo*);
using Visibility=void(*)(void*,uint64_t,uint64_t,const MethodInfo*);
using StepOne=void(*)(void*,float,float,int,const MethodInfo*);
using StepTwo=void(*)(void*,int,const MethodInfo*);
using Fog=float(*)(const MethodInfo*);
Preferred originalPreferred=nullptr;Visibility originalBack=nullptr,originalFront=nullptr;
StepOne originalStep1=nullptr;StepTwo originalStep2=nullptr;Fog originalFog=nullptr;
bool fail(std::string* error,const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    if(error)*error="[Native][ERROR][SurfaceBackground] Stage="+std::string(stage)+" Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}
void diagnostic(const std::string& error,int slot=-1,int operation=0) {
    static std::map<std::pair<int,int>,unsigned> counts;
    auto& count=counts[{slot,operation}];++count;
    if(count && !(count&(count-1))) {
        const auto it=entries.find(slot);const std::string owner=it==entries.end()?"":" Mod="+it->second.mod+" Class="+it->second.name+" ContentId="+std::to_string(slot);
        if(error.find("[WARN]")!=error.npos)BL_WARN("%s%s SuppressedTotal=%u",error.c_str(),owner.c_str(),count-1);
        else BL_ERROR("%s%s SuppressedTotal=%u",error.c_str(),owner.c_str(),count-1);
    }
}
void release(Layer& layer) {
    if(layer.root)il2cpp::api().gchandle_free(layer.root);
    if(layer.lease)backgroundAssets::release(layer.lease);
    layer={};
}
void assetRetiring(int slot) {
    std::lock_guard lock(mutex);
    for(auto& [id,e]:entries){(void)id;for(const auto& l:e.layers)if(l.slot==slot)e.unavailable=true;}
    awake.store(true,std::memory_order_release);
}
bool primitive(Il2CppClass* c,const char* expected,int size) {
    auto& a=il2cpp::api();uint32_t alignment=0;
    return c && P::name(c)==expected && a.class_is_valuetype(c) && a.class_value_size(c,&alignment)==size;
}
enum class Shape { Void,Int,Float,NullableInt,NullableFloat };
bool shape(const Il2CppType* type,Shape wanted,Refs* staged,std::string* detail) {
    auto& a=il2cpp::api();char* name=type?a.type_get_name(type):nullptr;
    const bool indirect=!name || std::strchr(name,'&') || std::strchr(name,'*');if(name)a.il2cpp_free(name);
    if(indirect){*detail="Expected=DirectValue Actual=MissingTypeNameOrByRefOrPointer";return false;}
    auto* c=a.class_from_il2cpp_type(type);
    if(wanted==Shape::Void || wanted==Shape::Int || wanted==Shape::Float) {
        const char* expected=wanted==Shape::Void?"System.Void":wanted==Shape::Int?"System.Int32":"System.Single";
        const bool valid=wanted==Shape::Void?P::name(c)==expected:primitive(c,expected,4);
        if(!valid){uint32_t align=0;const int size=c && a.class_is_valuetype(c)?a.class_value_size(c,&align):-1;
            *detail="Expected="+std::string(expected)+(wanted==Shape::Void?"":":4")+" Actual="+P::name(c)+":"+std::to_string(size);}
        return valid;
    }
    auto* value=il2cpp::findClassQuiet("System",wanted==Shape::NullableInt?"Int32":"Single");
    if(!primitive(value,wanted==Shape::NullableInt?"System.Int32":"System.Single",4)){*detail="Expected=NullablePayload:System.Int32OrSingle:4 Actual="+P::name(value);return false;}
    il2cpp::NullableLayout layout;
    if(!layout.bind(c,value,detail))return false;
    if(layout.size()!=8){*detail="Expected=NullableGprAggregate:8 Actual="+std::to_string(layout.size());return false;}
    uint32_t alignment=0;a.class_value_size(c,&alignment);
    if(alignment>8){*detail="Expected=NullableAlignmentAtMost8 Actual="+std::to_string(alignment);return false;}
    // Bool + 32-bit payload is a non-HFA eight-byte aggregate in AAPCS64.
    // Its bits travel in a GPR, including when T is Single. Never use an HFA
    // or assume the flag/value field order from a PC layout.
    (wanted==Shape::NullableInt?staged->nullableInt:staged->nullableFloat)=layout;
    return true;
}
const MethodInfo* method(Il2CppClass* cls,const char* name,bool stat,Shape result,std::initializer_list<Shape> args,Refs* staged,std::string* error) {
    auto& a=il2cpp::api();const MethodInfo* found=nullptr;void* it=nullptr;std::string mismatch;
    while(const auto* m=a.class_get_methods(cls,&it)) {
        const char* n=a.method_get_name(m);
        if(!n || std::strcmp(n,name) || a.method_get_param_count(m)!=args.size() || bool(a.method_get_flags(m,nullptr)&0x10u)!=stat ||
           (a.method_get_flags(m,nullptr)&0x400u) || a.method_is_generic(m) || !il2cpp::methodPointer(m))continue;
        Refs candidate=*staged;std::string reason;bool valid=shape(a.method_get_return_type(m),result,&candidate,&reason);unsigned i=0;
        if(!valid)mismatch="Return="+reason;
        for(auto arg:args) {
            if(!shape(a.method_get_param(m,i),arg,&candidate,&reason)){valid=false;mismatch="Parameter="+std::to_string(i)+" "+reason;}
            ++i;
        }
        if(!valid)continue;
        if(found){fail(error,"BindHooks","AmbiguousNativeOverload",name,"MultipleMatchingMethods");return nullptr;}
        *staged=candidate;found=m;
    }
    if(!found)fail(error,"BindHooks","NativeMethodContractMismatch",std::string(stat?"Static:":"Instance:")+name+":ExactTypesSizesStorageAndNullableGprAbi",mismatch.empty()?"MissingOrIncompatibleStorageBodyOrArity":mismatch);
    return found;
}
bool bind(std::string* error) {
    std::string ignored;if(!error)error=&ignored;
    if(refs.ready)return true;
    auto& a=il2cpp::api();std::string missing;
    const auto require=[&](auto fn,const char* name){if(!fn){if(!missing.empty())missing+=",";missing+=name;}};
    require(a.class_get_methods,"class_get_methods");require(a.method_get_name,"method_get_name");require(a.method_is_generic,"method_is_generic");
    require(a.gchandle_new,"gchandle_new");require(a.gchandle_get_target,"gchandle_get_target");require(a.gchandle_free,"gchandle_free");
    require(a.gc_wbarrier_set_field,"gc_wbarrier_set_field");require(a.field_static_set_value,"field_static_set_value");
    if(!missing.empty())return fail(error,"Bind","MetadataUnavailable","NativeMetadataAndGcExports","Missing="+missing);
    auto* main=il2cpp::findClassQuiet("Terraria","Main");auto* local=il2cpp::findClassQuiet("","LocalUserGameState");
    auto* id=il2cpp::findClassQuiet("Terraria.ID","SurfaceBackgroundID");auto* sets=id?il2cpp::findNested(id,"Sets"):nullptr;
    auto* assets=il2cpp::findClassQuiet("Terraria.GameContent","TextureAssets");
    auto* batch=il2cpp::findClassQuiet("Microsoft.Xna.Framework.Graphics","SpriteBatch");
    auto* sky=il2cpp::findClassQuiet("Terraria.Graphics.Effects","SkyManager");
    if(!main || !local || !id || !sets || !assets || !batch || !sky)
        return fail(error,"BindClasses","NativeClassMissing","Main,LocalUserGameState,SurfaceBackgroundID/Sets,TextureAssets,SpriteBatch,SkyManager",
                    std::string(!main?"Main ":"")+(!local?"LocalUserGameState ":"")+(!id?"SurfaceBackgroundID ":"")+(!sets?"SurfaceBackgroundID/Sets ":"")+(!assets?"TextureAssets ":"")+(!batch?"SpriteBatch ":"")+(!sky?"SkyManager":""));
    Refs r;P count;
    r.texture=il2cpp::findClassQuiet("Microsoft.Xna.Framework.Graphics","Texture2D");
    if(!r.texture)return fail(error,"BindClasses","NativeClassMissing","Microsoft.Xna.Framework.Graphics.Texture2D","Missing");
    if(!count.bind(id,"Count",K::Int32,true,error) || !count.read(nullptr,&r.vanilla,error))return false;
    if(r.vanilla<=0 || r.vanilla>512)return fail(error,"BindCount","InvalidNativeCount","SurfaceBackgroundID.Count:1..512",std::to_string(r.vanilla));
    if(!r.menu.bind(main,"gameMenu",K::Boolean,true,error) || !r.local.bind(local,"Instance",K::Reference,true,error,local) ||
       !r.instance.bind(main,"instance",K::Reference,true,error,main) ||
       !r.front.bind(main,local,"bgAlphaFrontLayer",error) || !r.far.bind(main,local,"bgAlphaFarBackLayer",error) ||
       !r.forest.bind(sets,"IsForest",K::BooleanArray,true,error) || !r.desert.bind(sets,"IsDesertVariant",K::BooleanArray,true,error) ||
       !r.style.bind(main,"bgStyle",K::Int32,true,error) || !r.styleRead.bind(main,"bgStyle",true,error) ||
       !r.speed.bind(main,"backgroundLayerTransitionSpeed",true,error) ||
       !r.baseColor.bind(main,"ColorOfSurfaceBackgroundsBase",K::Color,true,error) ||
       !r.modified.bind(main,"ColorOfSurfaceBackgroundsModified",K::Color,true,error) ||
       !r.batch.bind(main,"spriteBatch",K::Reference,true,error,batch) || !r.sky.bind(sky,"Instance",K::Reference,true,error,sky) ||
       !r.assets.bind(assets,"Background",K::AssetArray,true,error,content::textureAssetClass()) ||
       !r.widths.bind(main,"backgroundWidth",K::IntArray,true,error) || !r.heights.bind(main,"backgroundHeight",K::IntArray,true,error) ||
       !r.position.bind(main,"screenPosition",K::Vector2,true,error) || !r.screenWidth.bind(main,"screenWidth",K::Int32,true,error) ||
       !r.loops.bind(main,"bgLoops",false,error) || !r.start.bind(main,"bgStartX",false,error) || !r.top.bind(main,"bgTopY",false,error) ||
       !r.step.bind(main,"bgWidthScaled",true,error) || !r.scale.bind(main,"bgScale",true,error) ||
       !r.adjust.bind(main,"scAdj",false,error) || !r.surface.bind(main,"worldSurface",true,error))return false;
    if(r.baseColor.declared!=r.modified.property.declared)return fail(error,"BindColor","NativeColorTypeMismatch",P::name(r.baseColor.declared),P::name(r.modified.property.declared));
    if(!r.color.bind(r.baseColor.declared,error) || !r.renderer.bind(batch,sky,error))return false;
    il2cpp::ValueLayout vector;if(!il2cpp::resolveValueLayout(r.position.declared,&vector,error))return false;
    const char* fields[]={"X","Y"};size_t* offsets[]={&r.x,&r.y};
    for(size_t i=0;i<2;++i) {
        auto* f=il2cpp::findField(r.position.declared,fields[i]);auto* t=f?a.field_get_type(f):nullptr;
        char* name=t?a.type_get_name(t):nullptr;const bool indirect=!name || std::strchr(name,'&') || std::strchr(name,'*');if(name)a.il2cpp_free(name);
        if(!f || (a.field_get_flags(f)&0x10u) || indirect || !primitive(a.class_from_il2cpp_type(t),"System.Single",4) || !vector.offset(a.field_get_offset(f),4,offsets[i]))
            return fail(error,"BindPosition","NativeVectorLayoutMismatch",std::string("Vector2.")+fields[i]+":SingleWithin8Bytes","MissingWrongTypeStorageOrOffset");
    }
    if(r.x==r.y)return fail(error,"BindPosition","NativeVectorLayoutMismatch","DistinctVectorComponents","OverlappingFields");
    using S=Shape;
    if(!(r.preferred=method(main,"GetPreferredBGStyleForPlayer",true,S::Int,{},&r,error)) ||
       !(r.back=method(main,"UpdateBGVisibility_BackLayer",false,S::Void,{S::NullableInt,S::NullableFloat},&r,error)) ||
       !(r.frontMethod=method(main,"UpdateBGVisibility_FrontLayer",false,S::Void,{S::NullableInt,S::NullableFloat},&r,error)) ||
       !(r.modify=method(main,"DrawBG_ModifyBGFarBackLayerAlpha",false,S::Void,{S::Int,S::NullableInt,S::NullableFloat},&r,error)) ||
       !(r.step1=method(main,"DrawSurfaceBG_BackMountainsStep1",false,S::Void,{S::Float,S::Float,S::Int},&r,error)) ||
       !(r.step2=method(main,"DrawSurfaceBG_BackMountainsStep2",false,S::Void,{S::Int},&r,error)) ||
       !(r.fog=method(main,"DrawSurfaceBG_GetFogPower",true,S::Float,{},&r,error)))return false;
    // Every nullable occurrence must use the same closed generic layouts.
    auto native=[&](const MethodInfo* m,unsigned index){return a.class_from_il2cpp_type(a.method_get_param(m,index));};
    if(native(r.back,0)!=native(r.frontMethod,0) || native(r.back,0)!=native(r.modify,1) ||
       native(r.back,1)!=native(r.frontMethod,1) || native(r.back,1)!=native(r.modify,2))
        return fail(error,"BindHooks","NativeNullableTypeMismatch","ConsistentClosedNullableIntAndSingle","DifferentClosedGenericClasses");
    r.ready=true;refs=r;return true;
}
struct State {Il2CppObject* local=nullptr;Il2CppArray *front=nullptr,*far=nullptr;bool menu=false;int style=0;};
struct StateRoots {
    std::array<uint32_t,3> roots{};
    ~StateRoots(){for(auto id:roots)if(id)il2cpp::api().gchandle_free(id);}
    bool retain(const State& state,std::string* error) {
        const std::array<Il2CppObject*,3> objects{state.local,reinterpret_cast<Il2CppObject*>(state.front),reinterpret_cast<Il2CppObject*>(state.far)};
        for(size_t i=0;i<objects.size();++i)if(objects[i]) {
            roots[i]=il2cpp::api().gchandle_new(objects[i],false);
            if(!roots[i])return fail(error,"RetainDrawState","GcRootAllocationFailed","RootedLocalStateAndFadeArrays","RootIndex:"+std::to_string(i));
        }
        return true;
    }
};
bool readState(State* s,std::string* error) {
    double style=0;
    if(!refs.menu.read(nullptr,&s->menu,error) || !refs.local.read(nullptr,&s->local,error) || !refs.styleRead.read(nullptr,&style,error))return false;
    if(style<0 || style>16384 || std::floor(style)!=style)return fail(error,"ReadState","InvalidStyleId","NonNegativeInt32Style",std::to_string(style));
    s->style=int(style);
    return refs.front.field.contract().read(refs.front.local?s->local:nullptr,&s->front,error) && refs.far.field.contract().read(refs.far.local?s->local:nullptr,&s->far,error) &&
        refs.front.field.contract().array(s->front,size_t(refs.vanilla),error) && refs.far.field.contract().array(s->far,size_t(refs.vanilla),error);
}
bool ensure(State* s,std::string* error) {
    struct Generation {uint64_t epoch;Il2CppObject* local;} generation{epoch.load(),s->local};
    const auto current=[](void* data){auto* g=static_cast<Generation*>(data);Il2CppObject* local=nullptr;std::string ignored;
        return epoch.load()==g->epoch && refs.local.read(nullptr,&local,&ignored) && local==g->local;};
    const std::array<TableTarget,4> targets{{{&refs.front.field,refs.front.local?s->local:nullptr},{&refs.far.field,refs.far.local?s->local:nullptr},{&refs.forest,nullptr},{&refs.desert,nullptr}}};
    return growTables(targets,size_t(next),error,current,&generation) && readState(s,error);
}
double alpha(const State& state,int slot,bool front) {
    auto* array=front?state.front:state.far;
    return slot>=0 && size_t(slot)<array->length?static_cast<float*>(arrayData(array))[slot]:0;
}
bool request(Entry& entry,const Request& query,Reply* reply,std::unique_lock<std::recursive_mutex>& lock,std::string* error) {
    auto callback=providers[int(entry.language)].invoke;
    if(!callback)return fail(error,"Invoke","ProviderUnavailable","InstalledLanguageProvider",entry.mod+"/"+entry.name);
    const auto serial=epoch.load();++entry.inflight;lock.unlock();const bool result=callback(query,reply,error);lock.lock();--entry.inflight;
    if(epoch.load()!=serial || entry.retired)return fail(error,"Invoke","StaleWorldOrRetiredStyle","CurrentWorldAndLiveStyle",entry.mod+"/"+entry.name);
    if(!result && error && error->empty())fail(error,"Invoke","CallbackRejected","SuccessfulCallbackReply","RejectedWithoutDetail");
    return result;
}
int selected(bool menu,int vanilla,std::unique_lock<std::recursive_mutex>& lock) {
    Candidate best;const Entry* owner=nullptr;const auto serial=epoch.load();
    const auto offer=[&](Candidate c) {
        auto it=entries.find(c.slot);if(it==entries.end() || it->second.retired || it->second.unavailable || c.priority<0 || c.priority>8 || !std::isfinite(c.weight))return;
        c.weight=std::clamp(c.weight,0.0,1.0);auto& e=it->second;
        const bool earlier=owner && std::pair(e.mod,e.name)<std::pair(owner->mod,owner->name);
        const double rank=c.priority+c.weight,oldRank=best.priority+best.weight;
        if(best.slot<0 || rank>oldRank || (rank==oldRank && earlier)){best=c;owner=&e;}
    };
    std::array<Candidate,2> external{};size_t count=0;
    const std::array snapshot{providers[0],providers[1]};
    for(const auto& provider:snapshot) {
        const auto choose=provider.choose;const auto theme=provider.menu;
        if((menu && !theme) || (!menu && !choose))continue;
        lock.unlock();const auto result=menu?Candidate{theme(),8,.5}:choose();lock.lock();
        if(serial!=epoch.load())return vanilla;
        external[count++]=result;
    }
    // Evaluate current ownership only after all foreign callbacks complete.
    // A callback may retire the previous winner or activate another mod.
    for(auto& [id,e]:entries)if(!e.retired && (menu?e.menu:e.active))offer({id,e.priority,e.weight});
    for(size_t i=0;i<count;++i)offer(external[i]);
    return best.slot>=0 && !entries.at(best.slot).retired && !entries.at(best.slot).unavailable &&
        (menu || priorityAllows(best.priority,vanilla))?best.slot:vanilla;
}
void sweep(const State& state,bool selectedMod) {
    bool visible=false;
    for(auto& [id,e]:entries) {
        const bool front=alpha(state,id,true)>0,far=alpha(state,id,false)>0;
        visible|=front || far;
        if(!e.inflight) {
            if(!far){release(e.layers[0]);release(e.layers[1]);}
            if(!front)release(e.layers[2]);
        }
    }
    if(!selectedMod && !visible && state.style<refs.vanilla)awake.store(false,std::memory_order_release);
}
bool resolveTexture(Layer* cache,int slot,std::string* error) {
    if(slot<0)return fail(error,"ResolveTexture","InvalidTextureSlot","NonNegativeBackgroundSlot",std::to_string(slot));
    if(backgroundAssets::retired(slot))return fail(error,"ResolveTexture","AssetOwnerUnloaded","LiveBackgroundTexture",std::to_string(slot));
    Il2CppArray *assets=nullptr,*widths=nullptr,*heights=nullptr;
    if(!refs.assets.read(nullptr,&assets,error) || !refs.widths.read(nullptr,&widths,error) || !refs.heights.read(nullptr,&heights,error) ||
       !refs.assets.array(assets,size_t(slot)+1,error) || !refs.widths.array(widths,size_t(slot)+1,error) || !refs.heights.array(heights,size_t(slot)+1,error))return false;
    auto* asset=static_cast<Il2CppObject**>(arrayData(assets))[slot];auto* image=asset?content::textureAssetValue(asset):nullptr;
    auto& a=il2cpp::api();
    if(!image || !a.class_is_assignable_from(refs.texture,a.object_get_class(image)))return fail(error,"ResolveTexture","AssetMissingOrWrongTextureType","LoadedTexture2D",image?P::name(a.object_get_class(image)):"NullAssetOrValue");
    const TextureSize dimensions{static_cast<int32_t*>(arrayData(widths))[slot],static_cast<int32_t*>(arrayData(heights))[slot]};
    if(dimensions.width<=0 || dimensions.height<=0 || dimensions.width>32768 || dimensions.height>32768)
        return fail(error,"ResolveTexture","InvalidTextureDimensions","PositiveBoundedWidthAndHeight",std::to_string(dimensions.width)+"x"+std::to_string(dimensions.height));
    if(cache->slot==slot && cache->root && a.gchandle_get_target(cache->root)==image){cache->size=dimensions;return true;}
    const auto root=a.gchandle_new(image,false);
    if(!root)return fail(error,"RetainTexture","GcRootAllocationFailed","RootedBackgroundTexture",std::to_string(slot));
    const int lease=backgroundAssets::retain(slot);release(*cache);cache->root=root;cache->lease=lease;cache->slot=slot;cache->size=dimensions;return true;
}
bool color(Color* value,std::string* error) {
    uint32_t raw=0;if(!refs.baseColor.read(nullptr,&raw,error))return false;
    std::array<uint8_t,4> bytes{};std::memcpy(bytes.data(),&raw,4);
    *value={bytes[refs.color.channels[0]],bytes[refs.color.channels[1]],bytes[refs.color.channels[2]],bytes[refs.color.channels[3]]};return true;
}
bool paint(Entry& e,Layer& layer,const DrawPlan& plan,Color tint,std::unique_lock<std::recursive_mutex>& lock,std::string* error,bool publishColor=true) {
    struct Flight {Entry& e;explicit Flight(Entry& entry):e(entry){++e.inflight;}~Flight(){--e.inflight;}} flight(e);
    const auto serial=epoch.load();
    auto& a=il2cpp::api();Il2CppObject *batch=nullptr,*sky=nullptr;
    if(!refs.batch.read(nullptr,&batch,error) || (plan.drawSky && !refs.sky.read(nullptr,&sky,error)))return false;
    const uint32_t packed=refs.color.pack(tint.r,tint.g,tint.b,tint.a);
    if(publishColor && !refs.modified.write(nullptr,&packed,4,error))return false;
    if(serial!=epoch.load())return fail(error,"Draw","DrawSessionExpired","CurrentEngineDrawSession","WorldChangedDuringStateRead");
    auto* image=a.gchandle_get_target(layer.root);
    lock.unlock();
    const bool result=refs.renderer.draw(batch,sky,image,plan,tint,error,[](void* data){return epoch.load()==*static_cast<const uint64_t*>(data);},const_cast<uint64_t*>(&serial));
    lock.lock();return result;
}
void draw(void* self,Operation operation,const Step& stepState={}) {
    std::unique_lock flight(drawTransaction,std::try_to_lock);
    if(!flight){std::lock_guard lock(mutex);diagnostic("[Native][WARN][SurfaceBackground] Stage=Draw Result=OriginalLayersPreserved Reason=DrawDispatcherBusy",-1,13);return;}
    std::unique_lock lock(mutex);std::string error;State state;
    if(!readState(&state,&error) || !ensure(&state,&error)){diagnostic(error);return;}
    StateRoots stateRoots;if(!stateRoots.retain(state,&error)){diagnostic(error);return;}
    const auto serial=epoch.load();const int menu=state.menu?selected(true,-1,lock):-1;
    if(serial!=epoch.load())return;
    if(state.menu && menu<0){sweep(state,false);return;}
    Color base;if(!color(&base,&error)){diagnostic(error);return;}
    for(auto& [id,e]:entries) {
        if(serial!=epoch.load())return;
        const bool close=operation==Operation::Close;const double amount=alpha(state,id,close);
        if(!(amount>0))continue;
        const unsigned layerIndex=close?2:operation==Operation::Far?0:1;auto& layer=e.layers[layerIndex];
        Request q;q.slot=id;q.operation=operation;q.alpha=amount;q.color=base;
        if(close) {
            std::array<unsigned char,8> position{};float x=0,y=0;
            if(!refs.position.read(nullptr,position.data(),&error) || !refs.screenWidth.read(nullptr,&q.close.screenWidth,&error) ||
               !refs.surface.read(nullptr,&q.close.worldSurface,&error) || !refs.adjust.read(self,&q.close.scAdj,&error)){diagnostic(error,id,3);continue;}
            std::memcpy(&x,position.data()+refs.x,4);std::memcpy(&y,position.data()+refs.y,4);
            q.close.screenX=x;q.close.screenY=y;q.close.stepTop=stepState.top;q.close.push=stepState.push;
        } else {
            double loops=0;
            if(!refs.loops.read(self,&loops,&error) || !refs.start.read(self,&q.back.startX,&error) || !refs.top.read(self,&q.back.topY,&error) ||
               !refs.step.read(nullptr,&q.back.step,&error) || !refs.scale.read(nullptr,&q.back.scale,&error)){diagnostic(error,id,3);continue;}
            if(!std::isfinite(loops) || loops<0 || loops>32770 || std::floor(loops)!=loops){fail(&error,"ReadGeometry","InvalidLoopCount","Integer:0..32770",std::to_string(loops));diagnostic(error,id,3);continue;}
            q.back.loops=int(loops);
        }
        if(!e.retired) {
            Reply reply;
            if(!request(e,q,&reply,lock,&error)){diagnostic(error,id,4+int(operation));if(serial!=epoch.load())return;continue;}
            if(serial!=epoch.load())return;
            if(reply.custom.size()>128){fail(&error,"CustomDraw","DrawCommandCapacityExceeded","AtMost128Commands",std::to_string(reply.custom.size()));diagnostic(error,id,8);continue;}
            for(const auto& command:reply.custom) {
                Layer image;
                if(command.plan.loops!=1 || command.plan.drawSky) {fail(&error,"CustomDraw","InvalidCustomDrawPlan","OneSpriteWithoutSkySideEffects","LoopCountOrSkyRequested");diagnostic(error,id,8);continue;}
                if(!resolveTexture(&image,command.texture,&error)){diagnostic(error,id,8);continue;}
                DrawPlan plan=command.plan;
                if(!plan.hasSource){plan.width=image.size.width;plan.height=image.size.height;}
                if(plan.hasSource && (int64_t(plan.sourceX)+plan.width>image.size.width || int64_t(plan.sourceY)+plan.height>image.size.height)) {
                    release(image);fail(&error,"CustomDraw","SourceRectangleOutOfTexture","RectangleWithinTexture","OutsideTexture");diagnostic(error,id,8);continue;
                }
                if(!paint(e,image,plan,command.color,lock,&error,false))diagnostic(error,id,8);
                release(image);if(serial!=epoch.load())return;
            }
            if(close && !reply.defaultClose){layer.defaultClose=false;continue;}
            if(reply.texture<0)continue;
            if(!resolveTexture(&layer,reply.texture,&error)) {
                const bool outgoing=e.unavailable && layer.root;
                diagnostic(error+(outgoing?" Consequence=UsingRetainedOutgoingLayer":" Consequence=LayerSkipped"),id,9);
                if(!outgoing)continue;
            } else {layer.defaultClose=reply.defaultClose;layer.close=reply.close;}
        }
        if(!layer.root || (close && !layer.defaultClose))continue;
        DrawPlan plan;const bool valid=close?closePlan(layer.size,layer.close,q.close,&plan,&error):backPlan(layer.size,q.back,&plan,&error);
        if(!valid || !paint(e,layer,plan,multiply(base,amount),lock,&error))diagnostic(error,id,10);
        if(serial!=epoch.load())return;
    }
    // Current arrays can be recreated or modified during a foreign draw hook.
    if(readState(&state,&error))sweep(state,state.menu?menu>=0:state.style>=refs.vanilla);
    else diagnostic(error);
}
int hkPreferred(const MethodInfo* m) {
    const int vanilla=originalPreferred(m);
    if(!hooksReady.load() || !awake.load())return vanilla;
    std::unique_lock lock(mutex);State state;std::string error;
    if(!readState(&state,&error)){diagnostic(error);return vanilla;}
    if(state.menu)return vanilla;
    const int chosen=selected(false,vanilla,lock);
    if((chosen>=refs.vanilla || state.style>=refs.vanilla) && !ensure(&state,&error)){diagnostic(error);return vanilla;}
    return chosen;
}
bool visibility(uint64_t target,uint64_t amount,int* style,float* speed,bool* hasTarget,bool* hasAmount,std::string* error) {
    return refs.nullableInt.decode(&target,8,hasTarget,style,4,error) && refs.nullableFloat.decode(&amount,8,hasAmount,speed,4,error);
}
void hkBack(void* self,uint64_t target,uint64_t amount,const MethodInfo* m) {
    if(!hooksReady.load() || !awake.load()){originalBack(self,target,amount,m);return;}
    std::unique_lock lock(mutex);State state;std::string error;int style=0;float speed=0;bool hasTarget=false,hasAmount=false;
    if(!visibility(target,amount,&style,&speed,&hasTarget,&hasAmount,&error) || !readState(&state,&error) || !ensure(&state,&error)) {
        diagnostic(error);lock.unlock();originalBack(self,target,amount,m);return;
    }
    const auto serial=epoch.load();
    if(state.menu && !hasTarget) {
        const int theme=selected(true,-1,lock);
        if(serial!=epoch.load()){lock.unlock();originalBack(self,target,amount,m);return;}
        if(theme>=refs.vanilla) {
            if(!refs.style.write(nullptr,&theme,4,&error)){diagnostic(error);lock.unlock();originalBack(self,target,amount,m);return;}
            state.style=theme;
        }
    }
    if(!hasTarget)style=state.style;
    if(!entries.contains(style)){lock.unlock();originalBack(self,target,amount,m);return;}
    il2cpp::NullableLayout::Buffer empty;int zero=0;
    if(!refs.nullableInt.encode(false,&zero,4,&empty,&error)){diagnostic(error,style,11);lock.unlock();originalBack(self,target,amount,m);return;}
    void* args[]={&style,empty.data(),&amount};Il2CppObject* exception=nullptr;
    lock.unlock();il2cpp::api().runtime_invoke(refs.modify,self,args,&exception);lock.lock();
    if(exception){char detail[2048]{};auto& a=il2cpp::api();if(a.format_exception)a.format_exception(exception,detail,sizeof(detail));detail[sizeof(detail)-1]=0;
        fail(&error,"BackLayer","NativeAlphaRoutineException","NoException",detail[0]?detail:P::name(a.object_get_class(exception)));diagnostic(error,style,11);}
}
void hkFront(void* self,uint64_t target,uint64_t amount,const MethodInfo* m) {
    originalFront(self,target,amount,m);
    if(!hooksReady.load() || !awake.load())return;
    std::unique_lock lock(mutex);State state;std::string error;int style=0;float speed=0;bool hasTarget=false,hasAmount=false;
    if(!visibility(target,amount,&style,&speed,&hasTarget,&hasAmount,&error) || !readState(&state,&error) || !ensure(&state,&error)){diagnostic(error);return;}
    if(!hasTarget)style=state.style;
    const bool shown=!state.menu || selected(true,-1,lock)>=refs.vanilla;auto it=entries.find(style);
    if(!shown || it==entries.end() || it->second.retired){sweep(state,false);return;}
    Request q;q.slot=style;q.operation=Operation::Fades;
    if(hasAmount)q.speed=speed;else if(!refs.speed.read(nullptr,&q.speed,&error)){diagnostic(error,style,12);return;}
    const auto serial=epoch.load();auto* array=state.front;const size_t length=array->length;
    StateRoots stateRoots;if(!stateRoots.retain(state,&error)){diagnostic(error,style,12);return;}
    auto* owner=state.local;
    if(!std::isfinite(q.speed)){fail(&error,"FrontLayer","InvalidTransitionSpeed","FiniteSpeed",std::to_string(q.speed));diagnostic(error,style,12);return;}
    auto* fades=static_cast<float*>(arrayData(array));q.fades.assign(fades,fades+length);Reply reply;
    if(!request(it->second,q,&reply,lock,&error)){diagnostic(error,style,12);return;}
    if(reply.fades.size()!=length || !std::all_of(reply.fades.begin(),reply.fades.end(),[](float value){return std::isfinite(value);})) {
        fail(&error,"ModifyFarFades","InvalidFadeReply","FiniteArrayLength:"+std::to_string(length),"Length:"+std::to_string(reply.fades.size()));diagnostic(error,style,12);return;
    }
    if(serial!=epoch.load() || !readState(&state,&error) || state.front!=array || state.local!=owner){fail(&error,"ModifyFarFades","StaleFadeSnapshot","CurrentWorldAndOriginalArray","WorldOrArrayReplaced");diagnostic(error,style,12);return;}
    std::memcpy(arrayData(state.front),reply.fades.data(),length*4);
}
void hkStep1(void* self,float top,float scale,int push,const MethodInfo* m) {
    originalStep1(self,top,scale,push,m);
    if(!hooksReady.load() || !awake.load())return;
    {std::lock_guard lock(mutex);step1State={top,push,epoch.load(),true};}
    draw(self,Operation::Far);
}
void hkStep2(void* self,int push,const MethodInfo* m) {originalStep2(self,push,m);if(hooksReady.load() && awake.load())draw(self,Operation::Middle);}
float hkFog(const MethodInfo* m) {
    if(hooksReady.load() && awake.load()) {
        Step step;Il2CppObject* self=nullptr;std::string error;
        {std::lock_guard lock(mutex);step=step1State;step1State.valid=false;}
        if(step.valid && step.epoch==epoch.load()) {
            if(refs.instance.read(nullptr,&self,&error))draw(self,Operation::Close,step);
            else {std::lock_guard lock(mutex);diagnostic(error);}
        }
    }
    return originalFog(m);
}
bool identity(const std::string& value,size_t max) {
    if(value.empty() || value.size()>max)return false;
    for(unsigned char c:value)if(c<32 || c==127 || c=='/' || c=='\\' || c=='|')return false;
    return true;
}
}
bool install(std::string* error) {
    std::lock_guard lock(mutex);if(hooksReady.load())return true;
    if(!bind(error))return false;
    if(!backgroundAssets::observeRetirement(assetRetiring))return fail(error,"Install","AssetObserverCapacityExceeded","AvailableAssetLifetimeObserver","Full");
    // Metadata is validated for all six methods before any detour is applied.
    // An interrupted install leaves transparent original-only wrappers; retry
    // resumes missing links instead of chaining our own wrapper a second time.
    const auto link=[&](auto original,const MethodInfo* method,auto detour,auto* target){return original || hook::install(method,detour,target);};
    if(!link(originalPreferred,refs.preferred,hkPreferred,&originalPreferred) || !link(originalBack,refs.back,hkBack,&originalBack) ||
       !link(originalFront,refs.frontMethod,hkFront,&originalFront) || !link(originalStep1,refs.step1,hkStep1,&originalStep1) ||
       !link(originalStep2,refs.step2,hkStep2,&originalStep2) || !link(originalFog,refs.fog,hkFog,&originalFog))
        return fail(error,"Install","NativeHookInstallRejected","SixInstalledBackgroundHooks","PartialInstallOriginalOnly");
    if(next<0)next=refs.vanilla;
    hooksReady.store(true);return true;
}
int vanillaCount(std::string* error){std::lock_guard lock(mutex);return bind(error)?refs.vanilla:-1;}
int reserve(const std::string& mod,const std::string& name,Language language,std::string* error) {
    if(!identity(mod,128) || !identity(name,240)){fail(error,"Reserve","InvalidIdentity","ModUidAndClassName",mod+"/"+name);return -1;}
    if(language!=Language::Managed && language!=Language::JavaScript){fail(error,"Reserve","InvalidBridgeLanguage","ManagedOrJavaScript",std::to_string(int(language)));return -1;}
    if(!install(error))return -1;
    std::lock_guard lock(mutex);const auto key=std::pair(mod,name);auto existing=keys.find(key);
    if(existing!=keys.end()) {
        auto& e=entries.at(existing->second);
        if(e.language!=language){fail(error,"Reserve","ContentOwnedByAnotherLanguage","SameLanguageForIdentity",mod+"/"+name);return -1;}
        e.retired=false;e.unavailable=false;return existing->second;
    }
    if(next>=4096){fail(error,"Reserve","StyleCapacityExceeded","TotalStylesBelow4096",std::to_string(next));return -1;}
    const int id=next++;entries.emplace(id,Entry{mod,name,language});keys.emplace(key,id);return id;
}
int find(const std::string& mod,const std::string& name) {
    std::lock_guard lock(mutex);auto it=keys.find({mod,name});return it==keys.end() || entries.at(it->second).retired?-1:it->second;
}
int count(){std::lock_guard lock(mutex);return int(entries.size());}
bool select(int slot,int priority,double weight,bool active,bool menu,std::string* error) {
    std::lock_guard lock(mutex);auto it=entries.find(slot);
    if(it==entries.end() || it->second.retired)return fail(error,"Select","StyleNotRegistered","LiveRegisteredStyle",std::to_string(slot));
    if(priority<0 || priority>8 || !std::isfinite(weight) || weight<0 || weight>1)
        return fail(error,"Select","InvalidPriorityOrWeight","Priority:0..8,Weight:0..1",std::to_string(priority)+","+std::to_string(weight));
    auto& e=it->second;e.priority=priority;e.weight=weight;(menu?e.menu:e.active)=active;awake.store(true);return true;
}
void wake(){awake.store(true,std::memory_order_release);}
void provider(Language language,Choose choose,Menu menu,Invoke invoke) {std::lock_guard lock(mutex);providers[int(language)]={choose,menu,invoke};wake();}
void retire(int slot){std::lock_guard lock(mutex);auto it=entries.find(slot);if(it!=entries.end()){it->second.retired=true;it->second.active=false;it->second.menu=false;wake();}}
void retireLanguage(Language language){std::lock_guard lock(mutex);for(auto& [id,e]:entries)if(e.language==language){(void)id;e.retired=true;e.active=false;e.menu=false;}wake();}
void resetWorld(){std::lock_guard lock(mutex);epoch.fetch_add(1);step1State.valid=false;for(auto& [id,e]:entries){(void)id;e.active=false;}wake();}
}
