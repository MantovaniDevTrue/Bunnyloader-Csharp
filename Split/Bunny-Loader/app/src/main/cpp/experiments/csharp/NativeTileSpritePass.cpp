#include "experiments/csharp/NativeTileSpritePass.h"
#include "experiments/csharp/NativeTileSpritePassQueue.h"
#include "experiments/csharp/NativeTileSpriteBatch.h"
#include "experiments/csharp/NativeTileSpriteBatchLifecycle.h"
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

void postDraw(Il2CppObject* renderer,bool solidLayer,bool forRenderTargets,
              bool intoRenderTargets,const MethodInfo* method) {
    originalPostDraw(renderer,solidLayer,forRenderTargets,intoRenderTargets,method);
    if(!ready||!pending.pending()||solidLayer||intoRenderTargets)return;
    auto& a=il2cpp::api();
    uint8_t menu=1;
    a.field_static_get_value(gameMenuField,&menu);
    if(!eligibleDrawPass(solidLayer,intoRenderTargets,menu!=0))return;
    Il2CppObject* batch=nullptr;
    a.field_static_get_value(spriteBatchField,&batch);
    bool started=false;
    if(!beginNativeTileBatch(batch,&started))return;
    uint64_t frame=0;
    Vec screen{};
    a.field_static_get_value(frameField,&frame);
    a.field_static_get_value(screenField,&screen);
    if(!std::isfinite(screen.x)||!std::isfinite(screen.y)) {
        endNativeTileBatch(batch,started);return;
    }
    std::array<Sprite,kMaxStagedSprites> sprites{};
    size_t count=0;
    if(!pending.take(frame,&sprites,&count)) {
        endNativeTileBatch(batch,started);return;
    }
    for(size_t i=0;i<count;++i)
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

void resetNativeTileSpritePass() {pending.clear();}
}
