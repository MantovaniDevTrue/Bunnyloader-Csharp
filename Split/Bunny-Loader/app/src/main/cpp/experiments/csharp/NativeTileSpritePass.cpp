#include "experiments/csharp/NativeTileSpritePass.h"
#include "experiments/csharp/NativeTileSpritePassQueue.h"
#include "experiments/csharp/NativeTileParticleQueue.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "experiments/csharp/NativeTileSpriteBatch.h"
#include "experiments/csharp/NativeTileSpriteBatchLifecycle.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/FieldContract.h"
#include "hook/HookManager.h"
#include <array>
#include <cmath>
#include <cstring>

namespace bl::csharp::tile_sprite {
namespace {
using DrawPass = void (*)(Il2CppObject*,bool,bool,bool,const MethodInfo*);
DrawPass originalPostDraw = nullptr;
SpritePassQueue pending;
tile_particle::ParticleQueue pendingParticles;
const MethodInfo* newDust=nullptr;
NativeColorLayout dustColor{};
FieldInfo* spriteBatchField=nullptr;
FieldInfo* gameMenuField=nullptr;
FieldInfo* frameField=nullptr;
FieldInfo* screenField=nullptr;
bool ready=false;
struct Vec {float x=0,y=0;};

bool matchesBool(const Il2CppType* type,const char* name) {
    auto& a=il2cpp::api();
    if(!type||!a.type_get_name||!a.il2cpp_free)return false;
    char* text=a.type_get_name(type);
    const bool equal=text&&std::strcmp(text,name)==0;
    if(text)a.il2cpp_free(text);
    return equal;
}

bool matchType(const Il2CppType* type,const char* name) {
    return matchesBool(type,name);
}

bool bindDust(std::string* error) {
    using namespace il2cpp;
    auto& a=api();
    if(newDust)return true;
    if(!a.class_get_method_from_name||!a.method_get_param_count||
       !a.method_get_param||!a.method_get_return_type||
       !a.method_is_instance||!a.runtime_invoke||
       !a.class_from_il2cpp_type)return false;
    auto* dust=findClass({"Terraria","Dust",{}});
    const auto* method=dust?a.class_get_method_from_name(dust,"NewDust",9):nullptr;
    if(!method||!methodPointer(method)||a.method_is_instance(method)||
       a.method_get_param_count(method)!=9||
       !matchType(a.method_get_return_type(method),"System.Int32")) {
        if(error)*error="Stage=BindDust Reason=NativeNewDustMissing";
        return false;
    }
    const char* params[]={
        "Microsoft.Xna.Framework.Vector2","System.Int32","System.Int32",
        "System.Int32","System.Single","System.Single","System.Int32",
        "Microsoft.Xna.Framework.Color","System.Single"
    };
    for(unsigned i=0;i<9;++i)
        if(!matchType(a.method_get_param(method,i),params[i])) {
            if(error)*error="Stage=BindDust Reason=SignatureMismatch";
            return false;
        }
    NativeColorLayout layout;
    if(!layout.bind(a.class_from_il2cpp_type(a.method_get_param(method,7)),error))
        return false;
    dustColor=layout;
    newDust=method;
    return true;
}

void emitDust(const tile_particle::Particle& particle) {
    if(!newDust||!tile_particle::valid(particle))return;
    auto& a=il2cpp::api();
    struct Position {float x,y;} pos{
        static_cast<float>(particle.x*16+particle.offsetX),
        static_cast<float>(particle.y*16+particle.offsetY)
    };
    int32_t width=particle.width,height=particle.height,type=particle.dustType;
    int32_t alpha=particle.alpha;
    float speedX=static_cast<float>(particle.speedXMilli)*0.001f;
    float speedY=static_cast<float>(particle.speedYMilli)*0.001f;
    float scale=static_cast<float>(particle.scaleMilli)*0.001f;
    uint32_t color=dustColor.fromArgb(particle.argb);
    void* args[]={&pos,&width,&height,&type,&speedX,&speedY,&alpha,&color,&scale};
    Il2CppObject* error=nullptr;
    a.runtime_invoke(newDust,nullptr,args,&error);
}

void postDraw(Il2CppObject* renderer,bool solidLayer,bool forRenderTargets,
              bool intoRenderTargets,const MethodInfo* method) {
    originalPostDraw(renderer,solidLayer,forRenderTargets,intoRenderTargets,method);
    if(!ready||solidLayer||intoRenderTargets)return;
    auto& a=il2cpp::api();
    uint8_t menu=1;
    a.field_static_get_value(gameMenuField,&menu);
    if(!eligibleDrawPass(solidLayer,intoRenderTargets,menu!=0))return;
    uint64_t frame=0;
    a.field_static_get_value(frameField,&frame);
    const bool hasSprites=pending.pendingFor(frame);
    const bool hasDust=pendingParticles.pendingFor(frame);
    if(!hasSprites&&!hasDust)return;
    if(hasDust) {
        std::array<tile_particle::Particle,tile_particle::kMaxFrameParticles> commands{};
        size_t count=0;
        if(pendingParticles.take(frame,&commands,&count))
            for(size_t i=0;i<count;++i)emitDust(commands[i]);
    }
    if(!hasSprites)return;
    Vec screen{};
    a.field_static_get_value(screenField,&screen);
    if(!std::isfinite(screen.x)||!std::isfinite(screen.y))return;
    Il2CppObject* batch=nullptr;
    a.field_static_get_value(spriteBatchField,&batch);
    bool started=false;
    if(!beginNativeTileBatch(batch,&started))return;
    std::array<Sprite,kMaxStagedSprites> sprites{};
    size_t count=0;
    if(!pending.take(frame,&sprites,&count)) {
        endNativeTileBatch(batch,started);return;
    }
    for(int phase=0;phase<=1;++phase)
        for(size_t i=0;i<count;++i)
            if(sprites[i].phase==phase)
                drawNativeTileSprite(batch,sprites[i],screen.x,screen.y);
    endNativeTileBatch(batch,started);
}
}

bool installNativeTileSpritePass(std::string* error) {
    using namespace il2cpp;
    if(ready)return true;
    auto& a=api();
    if(!a.class_get_methods||!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance||!a.field_static_get_value)
        return false;
    auto* tileDrawing=findClass({"Terraria.GameContent.Drawing","TileDrawing",{}});
    auto* main=findClass({"Terraria","Main",{}});
    if(!tileDrawing||!main)return false;
    const MethodInfo* post=nullptr;
    void* iterator=nullptr;
    while(const MethodInfo* next=a.class_get_methods(tileDrawing,&iterator)) {
        if(std::strcmp(a.method_get_name(next),"PostDrawTiles")!=0 ||
           a.method_get_param_count(next)!=3||!a.method_is_instance(next)||
           !matchesBool(a.method_get_return_type(next),"System.Void"))continue;
        bool valid=true;
        for(unsigned i=0;i<3;++i)
            valid=valid&&matchesBool(a.method_get_param(next,i),"System.Boolean");
        if(!valid)continue;
        if(post){if(error)*error="Stage=BindPostDrawTiles Reason=AmbiguousOverloads";return false;}
        post=next;
    }
    if(!post||!methodPointer(post)) {
        if(error)*error="Stage=BindPostDrawTiles Reason=MethodNotFound";
        return false;
    }
    std::string actual;
    FieldInfo *batch=nullptr,*menu=nullptr,*frame=nullptr,*screen=nullptr;
    if(!nativeFieldContract(main,"spriteBatch","Microsoft.Xna.Framework.Graphics.SpriteBatch",
          true,sizeof(void*),&batch,nullptr,&actual)||
       !nativeFieldContract(main,"gameMenu","System.Boolean",
          true,1,&menu,nullptr,&actual)||
       !nativeFieldContract(main,"GameUpdateCount","System.UInt64",
          true,8,&frame,nullptr,&actual)||
       !nativeFieldContract(main,"screenPosition","Microsoft.Xna.Framework.Vector2",
          true,8,&screen,nullptr,&actual)) {
        if(error)*error="Stage=BindPostDrawTiles Reason=MainFields:"+actual;
        return false;
    }
    if(!bindNativeSpriteBatch(error)||!bindNativeTileBatchLifecycle(error))return false;
    if(!hook::install(post,postDraw,&originalPostDraw)) {
        if(error)*error="Stage=BindPostDrawTiles Reason=HookFailed";
        return false;
    }
    spriteBatchField=batch;gameMenuField=menu;
    frameField=frame;screenField=screen;
    ready=true;
    return true;
}

bool stageNativeTileSprites(uint64_t frame,const Batch& batch) {
    return ready&&pending.stage(frame,batch);
}

bool bindNativeTileDust(std::string* error) {
    return bindDust(error);
}

bool stageNativeTileParticlesForCurrentFrame(const tile_particle::Batch& batch) {
    if(!ready||!frameField||!newDust)return false;
    uint64_t frame=0;
    il2cpp::api().field_static_get_value(frameField,&frame);
    return frame!=0&&pendingParticles.stage(frame,batch);
}

bool stageNativeTileSpritesForCurrentFrame(const Batch& batch) {
    if(!ready || !frameField)return false;
    for(size_t i=0;i<batch.count;++i) {
        const auto& s=batch.sprites[i];
        if(s.textureLayer!=0&&!runtime::modTileOverlayAvailable(s.type,s.textureLayer,
                s.frameX,s.frameY,s.width,s.height))return false;
    }
    uint64_t frame=0;
    il2cpp::api().field_static_get_value(frameField,&frame);
    return frame!=0 && pending.stage(frame,batch);
}

void resetNativeTileSpritePass() {
    pending.clear();
    pendingParticles.clear();
}
}
