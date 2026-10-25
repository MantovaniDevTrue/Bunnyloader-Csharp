#include "experiments/csharp/NativeVisualRenderer.h"
#include "experiments/csharp/NativeMountRegistry.h"
#include "experiments/csharp/NativeMountVisualPolicy.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "content/common/ContentAssets.h"
#include "mods/ModLoader.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include <array>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sys/stat.h>

namespace bl::csharp {
namespace visual_native {
using namespace il2cpp;
struct Vec {float x=0,y=0;};
template<class T>T get(const void* p,size_t offset){T result;std::memcpy(&result,static_cast<const char*>(p)+offset,sizeof(result));return result;}
template<class T>void put(void* p,size_t offset,T value){std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));}
struct Layout {
    Il2CppClass* type=nullptr;size_t bytes=0,header=0;bool value=false;
    bool init(Il2CppClass* cls,std::string* error) {
        if(!cls)return false;
        type=cls;value=api().class_is_valuetype(cls);
        if(value){ValueLayout l;if(!resolveValueLayout(cls,&l,error))return false;bytes=l.size;header=l.header;}
        else {bytes=api().class_instance_size(cls);header=0;}
        return bytes>0&&bytes<=65536;
    }
    bool field(const char* name,size_t width,size_t* out,Il2CppClass** fieldType=nullptr) const {
        auto& a=api();auto* f=findField(type,name);if(!f||(a.field_get_flags(f)&0x10u))return false;
        auto* cls=a.class_from_il2cpp_type(a.field_get_type(f));if(!cls)return false;uint32_t align=0;
        const auto actual=a.class_is_valuetype(cls)?static_cast<size_t>(a.class_value_size(cls,&align)):sizeof(void*);
        const auto offset=a.field_get_offset(f);
        if(actual!=width||offset<header||offset-header>bytes||width>bytes-(offset-header)||(!value&&offset<sizeof(Il2CppObject)))return false;
        if(fieldType)*fieldType=cls;
        *out=offset-header;return true;
    }
};
bool primitive(Il2CppClass* cls,const char* name,size_t bytes) {
    auto& a=api();uint32_t align=0;const char* ns=cls?a.class_get_namespace(cls):nullptr,*actual=cls?a.class_get_name(cls):nullptr;
    return ns&&actual&&!std::strcmp(ns,"System")&&!std::strcmp(name,actual)&&a.class_is_valuetype(cls)&&a.class_value_size(cls,&align)==static_cast<int>(bytes);
}
bool scalar(const Layout& l,const char* name,const char* type,size_t width,size_t* out) {
    Il2CppClass* cls=nullptr;return l.field(name,width,out,&cls)&&primitive(cls,type,width);
}
bool vec(const Layout& l,const char* name,size_t* out) {
    Il2CppClass* cls=nullptr;if(!l.field(name,8,out,&cls))return false;Layout v;std::string error;size_t x=0,y=0;
    return v.init(cls,&error)&&v.value&&scalar(v,"X","Single",4,&x)&&scalar(v,"Y","Single",4,&y)&&x==0&&y==4;
}
struct Refs {
    Layout info,player,data,rectangle;
    size_t actor=0,cache=0,count=0,shadow=0,position=0,effect=0;
    size_t index=0,dead=0,invis=0,width=0,height=0,direction=0,rotation=0,world=0,velocity=0,mount=0,frame=0,frameHeight=0;
    size_t texture=0,drawPosition=0,source=0,color=0,drawRotation=0,origin=0,scale=0,drawEffect=0,shader=0,sourceValue=0,hasSource=0;
    bool nullableSource=false;NativeColorLayout nativeColor;
    Il2CppClass* cacheClass=nullptr;FieldInfo* screen=nullptr;
    const MethodInfo* mounted=nullptr;
    const MethodInfo* screenGetter=nullptr;
    std::vector<size_t> references;
};
Refs refs;
using LayerFn=void (*)(void*,const MethodInfo*);
LayerFn originalBack=nullptr,originalShield=nullptr,originalBarrier=nullptr;
bool backHooked=false,shieldHooked=false,barrierHooked=false,ready=false;
std::mutex statusMutex;NativeVisualRendererStatus status;
std::atomic<uint64_t> drawn{0},rejected{0};
std::atomic<bool> requested{false},startupReady{false};
std::mutex requestMutex;std::set<std::string> requiredTextures;
struct Asset {
    uint32_t root=0;int width=0,height=0;uint64_t used=0;
    ~Asset(){if(root){auto* asset=api().gchandle_get_target(root);if(asset)runtime::content::destroyTextureAsset(asset);api().gchandle_free(root);}}
    Il2CppObject* value() const {auto* asset=api().gchandle_get_target(root);return asset?runtime::content::textureAssetValue(asset):nullptr;}
};
std::map<std::string,std::unique_ptr<Asset>> assets;
uint64_t tick=0,pulse=0;
struct Point {float x=0,y=0;uint64_t tick=0;};
struct Trail {std::array<Point,6> points{};size_t size=0;};
std::array<Trail,256> trails;
struct Context {
    void* info=nullptr;Il2CppObject* player=nullptr;Il2CppArray* cache=nullptr;
    int id=0,mode=0,direction=1,effect=0,width=0,height=0,frameHeight=0;
    float rotation=0;Vec position,world,velocity,screen;
    bool mounted=false,trailSeen=false;
};
bool context(void* info,int mode,Context* out) {
    if(!ready||!info||get<float>(info,refs.shadow)!=0)return false;
    auto* player=get<Il2CppObject*>(info,refs.actor);
    if(!player||api().object_get_class(player)!=refs.player.type||get<uint8_t>(player,refs.dead)||get<uint8_t>(player,refs.invis))return false;
    const int id=get<int32_t>(player,refs.index);if(id<0||id>=256)return false;
    auto* cache=get<Il2CppArray*>(info,refs.cache);
    if(!cache||api().object_get_class(reinterpret_cast<Il2CppObject*>(cache))!=refs.cacheClass||cache->length>65536)return false;
    out->info=info;out->player=player;out->cache=cache;out->id=id;out->mode=mode;
    out->width=get<int32_t>(player,refs.width);out->height=get<int32_t>(player,refs.height);out->direction=get<int32_t>(player,refs.direction);
    out->position=get<Vec>(info,refs.position);out->world=get<Vec>(player,refs.world);out->velocity=get<Vec>(player,refs.velocity);
    if(refs.screen)api().field_static_get_value(refs.screen,&out->screen);
    else if(refs.screenGetter){
        using ScreenFn=Vec (*)(const MethodInfo*);
        out->screen=reinterpret_cast<ScreenFn>(methodPointer(refs.screenGetter))(refs.screenGetter);
    }else return false;
    out->rotation=get<float>(player,refs.rotation);out->frameHeight=get<int32_t>(player,refs.frame+refs.frameHeight);out->effect=get<int32_t>(info,refs.effect);
    auto* mount=get<Il2CppObject*>(player,refs.mount);
    if(mount){using Fn=bool (*)(Il2CppObject*,const MethodInfo*);out->mounted=reinterpret_cast<Fn>(methodPointer(refs.mounted))(mount,refs.mounted);}
    return out->width>0&&out->width<4096&&out->height>0&&out->height<4096&&std::isfinite(out->world.x)&&std::isfinite(out->world.y)&&
        std::isfinite(out->velocity.x)&&std::isfinite(out->velocity.y)&&std::isfinite(out->position.x)&&std::isfinite(out->position.y)&&std::isfinite(out->screen.x)&&std::isfinite(out->screen.y)&&std::isfinite(out->rotation);
}
uint32_t tint(const ManagedVisualCommand& command,float fraction) {
    const auto raw=command.color;const auto alpha=static_cast<uint32_t>(std::floor((raw>>24)*std::clamp(command.opacity*fraction,0.f,1.f)+0.5f));
    return (raw&0x00ffffffu)|(alpha<<24);
}
bool sprite(Context& ctx,Asset& asset,const ManagedVisualCommand& command,float x,float y,float scale,float opacity,int frame=0,float rotation=0) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(scale)||!std::isfinite(rotation))return false;
    const auto color=tint(command,opacity);if(!(color>>24))return false;
    int count=get<int32_t>(ctx.info,refs.count);
    if(count<0||static_cast<uintptr_t>(count)>=ctx.cache->length){rejected.fetch_add(1,std::memory_order_relaxed);return false;}
    std::array<unsigned char,1024> data{};if(refs.data.bytes>data.size())return false;
    auto* texture=asset.value();if(!texture)return false;
    const bool sheet=asset.height==asset.width*28;const int height=sheet?asset.width:asset.height;
    put(data.data(),refs.texture,texture);put(data.data(),refs.drawPosition,Vec{std::floor(x+0.5f),std::floor(y+0.5f)});
    int rectangle[]={0,sheet?std::clamp(frame,0,27)*height:0,asset.width,height};
    if(refs.nullableSource)put<uint8_t>(data.data(),refs.source+refs.hasSource,1);
    std::memcpy(data.data()+refs.source+refs.sourceValue,rectangle,sizeof(rectangle));
    put(data.data(),refs.color,refs.nativeColor.fromArgb(color));put(data.data(),refs.drawRotation,rotation);put(data.data(),refs.origin,Vec{asset.width*0.5f,height*0.5f});
    put(data.data(),refs.scale,Vec{scale,scale});put<int32_t>(data.data(),refs.drawEffect,ctx.effect);put<int32_t>(data.data(),refs.shader,0);
    auto* target=static_cast<char*>(arrayData(ctx.cache))+static_cast<size_t>(count)*refs.data.bytes;
    std::memcpy(target,data.data(),refs.data.bytes);
    for(auto offset:refs.references){auto* value=get<Il2CppObject*>(data.data(),offset);api().gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(ctx.cache),reinterpret_cast<void**>(target+offset),value);}
    put<int32_t>(ctx.info,refs.count,count+1);drawn.fetch_add(1,std::memory_order_relaxed);return true;
}
void draw(const ManagedVisualCommand& command,void* context) {
    auto& ctx=*static_cast<Context*>(context);const int style=command.style?command.style:(command.kind==ManagedVisualKind::PlayerOverlay?2:1);
    if(style!=ctx.mode)return;
    const auto found=assets.find(command.texture);if(found==assets.end())return;
    auto& asset=*found->second;const float x=ctx.position.x-ctx.screen.x+ctx.width*0.5f,y=ctx.position.y-ctx.screen.y+ctx.height*0.5f;
    if(style==1) {
        const float torso=ctx.position.y-ctx.screen.y+ctx.height-(ctx.frameHeight>0?ctx.frameHeight*0.5f:ctx.height*0.5f)+7;
        sprite(ctx,asset,command,x-ctx.direction*4,torso,std::clamp(command.scale,0.5f,1.5f),1,6,ctx.rotation);
    } else if(style==2) {
        const float wave=1+std::sin(static_cast<float>(++pulse)*0.09f)*0.035f,scale=std::clamp(command.scale,0.6f,1.4f);
        sprite(ctx,asset,command,x,y,scale*1.08f*wave,0.2f);sprite(ctx,asset,command,x,y,scale*wave,0.94f);
    } else if(style==3&&!ctx.trailSeen) {
        ctx.trailSeen=true;auto& trail=trails[static_cast<size_t>(ctx.id)];const auto now=++pulse;
        const float px=ctx.world.x+ctx.width*0.5f,py=ctx.world.y+ctx.height-3;
        const bool moving=std::abs(ctx.velocity.x)>1.25f&&std::abs(ctx.velocity.y)<0.3f&&!ctx.mounted;
        if(moving&&(!trail.size||std::abs(px-trail.points[trail.size-1].x)+std::abs(py-trail.points[trail.size-1].y)>6)) {
            if(trail.size==6){std::move(trail.points.begin()+1,trail.points.end(),trail.points.begin());--trail.size;}
            trail.points[trail.size++]={px,py,now};
        }
        while(trail.size&&now-trail.points[0].tick>16){std::move(trail.points.begin()+1,trail.points.begin()+static_cast<ptrdiff_t>(trail.size),trail.points.begin());--trail.size;}
        for(size_t i=0;i<trail.size;++i) {
            const auto& point=trail.points[i];const float life=std::max(0.f,1.f-static_cast<float>(now-point.tick)/17.f);
            sprite(ctx,asset,command,point.x-ctx.screen.x-ctx.direction*3,point.y-ctx.screen.y-5,
                std::clamp((0.35f+0.55f*life)*command.scale,0.3f,1.15f),life*0.72f);
        }
    }
}
void clearTrail(void* info) {
    if(!ready||!info)return;
    auto* player=get<Il2CppObject*>(info,refs.actor);
    if(!player||api().object_get_class(player)!=refs.player.type)return;
    const int id=get<int32_t>(player,refs.index);if(id>=0&&id<256)trails[static_cast<size_t>(id)].size=0;
}
void render(void* info,int mode) {
    if(!activeManagedVisualCount()){if(mode==3)clearTrail(info);return;}
    Context ctx;if(!context(info,mode,&ctx)){if(mode==3)clearTrail(info);return;}
    visitManagedVisualCommands(ctx.id,draw,&ctx,8);
    if(mode==3&&!ctx.trailSeen)trails[static_cast<size_t>(ctx.id)].size=0;
}

void renderMountBalloons(void* info) {
    if(!ready||!info)return;
    Context ctx;
    if(!context(info,3,&ctx)||!ctx.mounted||!ctx.player)return;
    std::array<float,3> rotations{};
    Il2CppObject* texture=nullptr;
    if(!nativeMountBalloonState(ctx.player,&rotations,&texture)||!texture)return;
    int count=get<int32_t>(info,refs.count);
    if(count<0||count+3>static_cast<int>(ctx.cache->length))return;
    const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const mount_visual::Point base{
        ctx.position.x-ctx.screen.x+ctx.width*0.5f,
        ctx.position.y-ctx.screen.y+ctx.height*0.5f
    };
    for(int i=0;i<3;++i) {
        const auto position=mount_visual::balloonAt(i,ctx.direction,millis,
            base,ctx.rotation,rotations[static_cast<size_t>(i)]);
        std::array<unsigned char,1024> data{};
        if(refs.data.bytes>data.size())return;
        put(data.data(),refs.texture,texture);
        put(data.data(),refs.drawPosition,Vec{position.position.x,position.position.y});
        int rectangle[]={28,position.frame*56,28,42};
        if(refs.nullableSource)put<uint8_t>(data.data(),refs.source+refs.hasSource,1);
        std::memcpy(data.data()+refs.source+refs.sourceValue,rectangle,sizeof(rectangle));
        put(data.data(),refs.color,refs.nativeColor.fromArgb(0xffffffffu));
        put(data.data(),refs.drawRotation,position.rotation);
        put(data.data(),refs.origin,Vec{position.origin.x,position.origin.y});
        put(data.data(),refs.scale,Vec{1.f,1.f});
        put<int32_t>(data.data(),refs.drawEffect,ctx.effect^1);
        put<int32_t>(data.data(),refs.shader,0);
        auto* target=static_cast<char*>(arrayData(ctx.cache))+
             static_cast<size_t>(count)*refs.data.bytes;
        std::memcpy(target,data.data(),refs.data.bytes);
        for(auto offset:refs.references) {
            auto* object=get<Il2CppObject*>(data.data(),offset);
            api().gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(ctx.cache),
                reinterpret_cast<void**>(target+offset),object);
        }
        ++count;
    }
    put<int32_t>(info,refs.count,count);
}
void back(void* info,const MethodInfo* m){
    render(info,3);
    renderMountBalloons(info);
    if(originalBack)originalBack(info,m);
}
void shield(void* info,const MethodInfo* m){if(originalShield)originalShield(info,m);render(info,1);}
void barrier(void* info,const MethodInfo* m){if(originalBarrier)originalBarrier(info,m);render(info,2);}
bool bind(std::string* error) {
    const auto unavailable=[&](const char* stage){if(error){auto previous=*error;*error="NativeDrawMetadataUnavailable:"+std::string(stage);if(!previous.empty())*error+=":"+previous;}return false;};
    auto& a=api();if(!a.class_get_rank||!a.object_get_class||!a.gchandle_new||!a.gchandle_free||!a.gchandle_get_target||!a.gc_wbarrier_set_field||!a.method_is_instance)return unavailable("Il2CppApi");
    auto* info=findClass({"Terraria.DataStructures","PlayerDrawSet",{}});auto* player=findClass({"Terraria","Player",{}});auto* data=findClass({"Terraria.DataStructures","DrawData",{}});
    if(!refs.info.init(info,error)||!refs.player.init(player,error)||!refs.data.init(data,error)||!refs.data.value||refs.data.bytes>1024||(refs.info.value&&refs.info.bytes<=16))return unavailable("DrawSetOrDrawDataLayout");
    Il2CppClass* actor=nullptr;
    if(!refs.info.field("drawPlayer",sizeof(void*),&refs.actor,&actor)||actor!=player||!refs.info.field("DrawDataCache",sizeof(void*),&refs.cache,&refs.cacheClass)||
       a.class_get_rank(refs.cacheClass)!=1||a.class_get_element_class(refs.cacheClass)!=data||!scalar(refs.info,"DrawDataCacheCount","Int32",4,&refs.count)||
       !scalar(refs.info,"shadow","Single",4,&refs.shadow)||!vec(refs.info,"Position",&refs.position))return unavailable("DrawSetShadowOrPosition");
    Il2CppClass* effect=nullptr;
    if(!refs.info.field("playerEffect",4,&refs.effect,&effect))return unavailable("PlayerEffectField");
    if(a.class_is_enum(effect))effect=a.class_from_il2cpp_type(a.class_enum_basetype(effect));
    if(!primitive(effect,"Int32",4))return unavailable("PlayerEffectEnum");
    for(const auto& [name,out]:std::initializer_list<std::pair<const char*,size_t*>>{{"whoAmI",&refs.index},{"width",&refs.width},{"height",&refs.height},{"direction",&refs.direction}})
        if(!scalar(refs.player,name,"Int32",4,out))return unavailable("PlayerIdentityFields");
    if(!scalar(refs.player,"dead","Boolean",1,&refs.dead)||!scalar(refs.player,"invis","Boolean",1,&refs.invis)||!scalar(refs.player,"bodyRotation","Single",4,&refs.rotation)||
       !vec(refs.player,"position",&refs.world)||!vec(refs.player,"velocity",&refs.velocity))return unavailable("PlayerStateFields");
    Il2CppClass* frame=nullptr,*mount=nullptr;
    if(!refs.player.field("bodyFrame",16,&refs.frame,&frame)||!refs.rectangle.init(frame,error)||!scalar(refs.rectangle,"Height","Int32",4,&refs.frameHeight)||
       !refs.player.field("mount",sizeof(void*),&refs.mount,&mount))return unavailable("BodyFrameOrMount");
    refs.mounted=findMethodBySignature(mount,parseSignature("bool get_Active()"));if(!refs.mounted||!a.method_is_instance(refs.mounted)||!methodPointer(refs.mounted))return unavailable("MountedActiveMethod");
    auto* main=findClass({"Terraria","Main",{}});
    refs.screen=main?findField(main,"screenPosition"):nullptr;
    refs.screenGetter=nullptr;
    if(!refs.screen||!(a.field_get_flags(refs.screen)&0x10u)){
        refs.screen=nullptr;
        const auto* getter=main?findMethodBySignature(main,parseSignature("Vector2 get_screenPosition()")):nullptr;
        if(!getter||a.method_is_instance(getter)||!methodPointer(getter))return unavailable("ScreenPositionFieldOrStaticGetter");
        refs.screenGetter=getter;
    }
    Layout screen;size_t sx=0,sy=0;
    auto* positionField=findField(info,"Position");
    auto* screenClass=refs.screen?a.class_from_il2cpp_type(a.field_get_type(refs.screen)):(positionField?a.class_from_il2cpp_type(a.field_get_type(positionField)):nullptr);
    if(!screen.init(screenClass,error)||!screen.value||screen.bytes!=8||!scalar(screen,"X","Single",4,&sx)||!scalar(screen,"Y","Single",4,&sy)||sx!=0||sy!=4)return unavailable("ScreenPositionLayout");
    Il2CppClass* texture=nullptr,*source=nullptr;
    if(!refs.data.field("texture",sizeof(void*),&refs.texture,&texture)||a.class_is_valuetype(texture)||!vec(refs.data,"position",&refs.drawPosition)||
       !vec(refs.data,"origin",&refs.origin)||!vec(refs.data,"scale",&refs.scale)||!scalar(refs.data,"rotation","Single",4,&refs.drawRotation)||!scalar(refs.data,"shader","Int32",4,&refs.shader))return unavailable("DrawDataFieldLayout");
    auto* sourceField=findField(data,"sourceRect");if(!sourceField)return unavailable("SourceRectField");source=a.class_from_il2cpp_type(a.field_get_type(sourceField));uint32_t align=0;
    if(!source||!refs.data.field("sourceRect",static_cast<size_t>(a.class_value_size(source,&align)),&refs.source))return unavailable("SourceRectLayout");
    Layout src;if(!src.init(source,error)||!src.value)return unavailable("SourceRectValueLayout");refs.nullableSource=src.bytes!=16;
    if(refs.nullableSource) {
        Il2CppClass* rectangle=nullptr;
        if(!scalar(src,"hasValue","Boolean",1,&refs.hasSource)||!src.field("value",16,&refs.sourceValue,&rectangle)||!refs.rectangle.init(rectangle,error))return unavailable("NullableSourceRectLayout");
    } else {refs.sourceValue=0;refs.rectangle=src;}
    for(const auto& [name,expected]:std::initializer_list<std::pair<const char*,size_t>>{{"X",0},{"Y",4},{"Width",8},{"Height",12}}) {
        size_t offset=0;if(!scalar(refs.rectangle,name,"Int32",4,&offset)||offset!=expected)return unavailable("RectangleFieldLayout");
    }
    Il2CppClass* color=nullptr,*drawEffect=nullptr;
    if(!refs.data.field("color",4,&refs.color,&color)||!a.class_is_valuetype(color)||!refs.data.field("effect",4,&refs.drawEffect,&drawEffect))return unavailable("DrawDataColorOrEffect");
    if(!refs.nativeColor.bind(color,error))return unavailable("DrawDataColorChannelLayout");
    if(a.class_is_enum(drawEffect))drawEffect=a.class_from_il2cpp_type(a.class_enum_basetype(drawEffect));
    refs.references.clear();
    if(!primitive(drawEffect,"Int32",4)||!valueReferenceOffsets(data,0,&refs.references,error)||refs.references.size()!=1||refs.references[0]!=refs.texture)return unavailable("DrawDataReferenceLayout");
    auto* layers=findClass({"Terraria.DataStructures","PlayerDrawLayers",{}});
    const auto layer=[&](const char* signature,LayerFn replacement,LayerFn* original,bool* hooked) {
        if(*hooked)return true;
        const auto* m=layers?findMethodBySignature(layers,parseSignature(signature)):nullptr;
        if(!m||a.method_is_instance(m)||!hook::install(m,replacement,original))return unavailable("DrawLayerMethodOrHook");
        *hooked=true;return true;
    };
    return layer("void DrawPlayer_10_BackAcc(PlayerDrawSet drawinfo)",back,&originalBack,&backHooked)&&
        layer("void DrawPlayer_25_Shield(PlayerDrawSet drawinfo)",shield,&originalShield,&shieldHooked)&&
        layer("void DrawPlayer_35_IceBarrier(PlayerDrawSet drawinfo)",barrier,&originalBarrier,&barrierHooked);
}
void countMissing(const ManagedVisualCommand& command,void* context){if(!assets.contains(command.texture))++*static_cast<size_t*>(context);}
void missing(const ManagedVisualCommand& command,void* context) {
    auto& list=*static_cast<std::vector<std::string>*>(context);const auto found=assets.find(command.texture);
    if(found!=assets.end()){found->second->used=tick;return;}
    if(list.size()<8&&std::find(list.begin(),list.end(),command.texture)==list.end())list.push_back(command.texture);
}
} // namespace visual_native
void requestNativeVisualRenderer(const std::string& texture) {
    using namespace visual_native;std::lock_guard lock(requestMutex);
    if(!texture.empty())requiredTextures.insert(texture);
    startupReady.store(false,std::memory_order_release);requested.store(true,std::memory_order_release);
}
size_t pendingNativeVisualRenderer(){return visual_native::requested.load(std::memory_order_acquire)&&!visual_native::startupReady.load(std::memory_order_acquire)?1:0;}
void pumpNativeVisualRendererOnGameThread() {
    using namespace visual_native;++tick;static bool hadVisuals=false;
    const bool active=activeManagedVisualCount()>0;
    if(!active&&startupReady.load(std::memory_order_acquire)) {if(hadVisuals)for(auto& trail:trails)trail.size=0;hadVisuals=false;return;}
    if(!active&&!requested.load(std::memory_order_acquire))return;
    hadVisuals=active;
    static uint64_t nextBindAttempt=0;
    std::string error;if(!ready) {
        if(tick<nextBindAttempt)return;
        ready=bind(&error);
        if(!ready){
            nextBindAttempt=tick+30;
            if(error.empty())error="NativeDrawMetadataUnavailable";
            std::lock_guard lock(statusMutex);
            if(status.error!=error)BL_WARN("[CSharp][Visual] Stage=Bind Result=Pending Reason=%s",error.c_str());
            status.error=error;return;
        }
    }
    std::vector<std::string> pending;
    {
        std::lock_guard lock(requestMutex);
        for(const auto& texture:requiredTextures) {
            const auto found=assets.find(texture);
            if(found!=assets.end())found->second->used=tick;
            else if(pending.size()<8)pending.push_back(texture);
        }
    }
    if(active)visitManagedVisualCommands(-1,missing,&pending,128);
    for(const auto& name:pending) {
        const auto colon=name.find(':');if(colon==std::string::npos)continue;
        const auto relative=name.substr(colon+1),modRoot=mods::rootOf(name.substr(0,colon));
        if(modRoot.empty()||!relative.starts_with("Content/")||relative.find("..")!=std::string::npos||relative.find('\\')!=std::string::npos)continue;
        const auto path=modRoot+"/content/"+relative;struct stat st{};if(stat(path.c_str(),&st)||!S_ISREG(st.st_mode)||st.st_size<1||st.st_size>1024*1024)continue;
        if(assets.size()>=64) {
            const auto old=std::min_element(assets.begin(),assets.end(),[](const auto& a,const auto& b){return a.second->used<b.second->used;});
            if(old==assets.end()||old->second->used==tick)continue;
            assets.erase(old);
        }
        auto* object=runtime::content::loadTextureAsset(path,nullptr,0,"CSharpVisual:"+name,nullptr,nullptr);
        auto asset=std::make_unique<Asset>();
        if(!object)continue;
        if(!runtime::content::textureAssetSize(object,&asset->width,&asset->height)||asset->width<1||asset->width>256||asset->height<1||asset->height>2048){runtime::content::destroyTextureAsset(object);continue;}
        asset->root=api().gchandle_new(object,false);asset->used=tick;if(!asset->root){runtime::content::destroyTextureAsset(object);continue;}assets[name]=std::move(asset);
    }
    size_t absent=0;if(active)visitManagedVisualCommands(-1,countMissing,&absent,128);
    {
        std::lock_guard lock(requestMutex);size_t requiredMissing=0;
        for(const auto& texture:requiredTextures)if(!assets.contains(texture))++requiredMissing;
        absent+=requiredMissing;startupReady.store(ready&&requiredMissing==0,std::memory_order_release);
    }
    std::lock_guard lock(statusMutex);status.ready=ready&&absent==0;status.error=absent?"TextureAssetsPending:"+std::to_string(absent):"";status.assets=assets.size();
}
void resetNativeVisualRenderer(){using namespace visual_native;for(auto& trail:trails)trail.size=0;pulse=0;}
NativeVisualRendererStatus nativeVisualRendererStatus(){std::lock_guard lock(visual_native::statusMutex);auto value=visual_native::status;value.drawn=visual_native::drawn.load(std::memory_order_relaxed);value.rejected=visual_native::rejected.load(std::memory_order_relaxed);return value;}
} // namespace bl::csharp
