#include "experiments/csharp/NativeTileGameplay.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include "experiments/csharp/NativeTileFrameTogglePolicy.h"
#include "experiments/csharp/NativeTileCampfirePolicy.h"
#include "experiments/csharp/NativeTileSmartInteractPolicy.h"
#include "experiments/csharp/NativeTileFramePolicy.h"
#include "experiments/csharp/NativeTileKillPolicy.h"
#include "experiments/csharp/NativeTileDustPolicy.h"
#include "experiments/csharp/NativeTileDropPolicy.h"
#include "experiments/csharp/NativeTileMiningPolicy.h"
#include "experiments/csharp/NativeTileSaplingPolicy.h"
#include "experiments/csharp/NativeTileAnimationPolicy.h"
#include "experiments/csharp/NativeTileScanPolicy.h"
#include "experiments/csharp/NativeTileDrawWire.h"
#include "experiments/csharp/NativeTileDrawBindings.h"
#include "experiments/csharp/NativeTileSpriteReply.h"
#include "experiments/csharp/NativeTileParticleReply.h"
#include "experiments/csharp/NativeTileSpritePass.h"
#include "il2cpp/FieldContract.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/tiles/TileAccess.h"
#include "content/tiles/ModTiles.h"
#include "content/items/ModItems.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>

namespace bl::csharp {
namespace {

struct TileBehavior {
    NativeTileGameplaySpec spec;
    int itemType = -1;
    int dustType = -1;
    std::map<int,int> styleDrops;
};
std::map<int, TileBehavior> g_tiles;
std::unordered_set<int> g_campfireTiles;
std::array<std::atomic<uint8_t>, 65536> g_managedDrawMarks{};
std::array<std::atomic<uint8_t>, 65536> g_managedDrawUncached{};
tile_draw::PublishedBindings<TileBehavior, 65536> g_managedDrawBindings;
bool g_managedDrawBound = false;
std::map<std::string,std::string> g_assemblyByMod;
bool g_wireInstalled = false;
bool g_overgroundInstalled = false;
bool g_undergroundInstalled = false;
bool g_interactInstalled=false;
bool g_slopeInstalled=false;
bool g_poundInstalled=false;
bool g_playerPlacementInstalled=false;
bool g_placeTileInstalled=false;
bool g_mouseOverInstalled=false;
bool g_mouseOverFarInstalled=false;
FieldInfo* g_tileTargetX=nullptr;
FieldInfo* g_tileTargetY=nullptr;
FieldInfo* g_mouseFrame=nullptr;
int g_cursorEnabledOffset=-1;
int g_cursorItemOffset=-1;
std::map<Il2CppObject*,uint64_t> g_mouseOverFrames;
std::map<Il2CppObject*,uint64_t> g_mouseOverFarFrames;
thread_local int g_playerPlaceDepth=0;
int g_useAttempt=-1,g_useRelease=-1,g_useHappened=-1;
FieldInfo* g_gameUpdateField=nullptr;
std::map<Il2CppObject*,uint64_t> g_lastUseFrames;
bool g_installed = false;
bool g_dropInstalled = false;
const MethodInfo* g_tileStyleMethod=nullptr;
Il2CppClass* g_tileStyleReturnClass=nullptr;
bool g_dustInstalled = false;
bool g_killInstalled = false;
bool g_makeDustInstalled = false;
bool g_frameInstalled = false;
bool g_canKillInstalled = false;
bool g_animationInstalled = false;
bool g_tileScanReady=false;
bool g_tileLightReady=false;
bool g_usesManagedNearby=false;
bool g_usesManagedLight=false;
FieldInfo* g_screenPositionField=nullptr;
FieldInfo* g_screenWidthField=nullptr;
FieldInfo* g_screenHeightField=nullptr;
FieldInfo* g_buffScanWidthField=nullptr;
FieldInfo* g_buffScanHeightField=nullptr;
const MethodInfo* g_addTileLight=nullptr;
uint64_t g_tileScanTick=0;
size_t g_tileScanVisualCursor=0,g_tileScanBuffCursor=0,g_tileScanLightCursor=0;
struct TileLightCache {int x=0,y=0,type=0,r=0,g=0,b=0;uint64_t frame=0;};
std::map<uint64_t,TileLightCache> g_tileLightCache;
std::map<uint64_t,uint64_t> g_tileNearbyEvents;
using SceneScanFn=void(*)(Il2CppObject*,const void*,const MethodInfo*);
SceneScanFn g_originalSceneScan=nullptr;
struct CampfireSceneBinding {
    bool ready=false;
    int centerOffset=0;
    int hasCampfireOffset=0;
    FieldInfo* scanWidth=nullptr;
    FieldInfo* scanHeight=nullptr;
};
CampfireSceneBinding g_campfireScene{};
struct CampfireSoundBinding {
    const MethodInfo* play=nullptr;
    FieldInfo* mech=nullptr;
    bool ready=false;
};
CampfireSoundBinding g_campfireSound{};
using SmartInteractFn=void(*)(Il2CppObject*,const void*,const MethodInfo*);
SmartInteractFn g_originalSmartInteract=nullptr;
struct SmartInteractBinding {
    const MethodInfo* join=nullptr;
    const MethodInfo* contains=nullptr;
    const MethodInfo* add=nullptr;
    int settingsOffsets[4]{};
    int targetsOffset=-1;
    bool ready=false;
};
SmartInteractBinding g_smartInteract{};
bool g_smartInteractRejected=false;

using AnimateFn = void (*)(const MethodInfo*);
AnimateFn g_originalAnimate = nullptr;
FieldInfo* g_frameArray = nullptr;
FieldInfo* g_counterArray = nullptr;
std::map<int, tile_animation::Policy> g_animatedTiles;
std::array<std::atomic<int32_t>,65536> g_animatedDrawFrames{};
thread_local int g_dustBudget = -1;
thread_local int g_dustOwner = -1;
thread_local bool g_frameCleanup = false;

using DropFn = void (*)(int32_t, int32_t, uint32_t, int32_t*, int32_t*,
                       int32_t*, int32_t*, bool*, bool, const MethodInfo*);
using KillFn = void (*)(int32_t, int32_t, bool, bool, bool, const MethodInfo*);
using DustFn = int32_t (*)(int32_t, int32_t, uint32_t, const MethodInfo*);
using KillSoundFn=void (*)(int32_t,int32_t,bool,uint32_t,const MethodInfo*);
using FrameFn = void (*)(int32_t, int32_t, int32_t, uint32_t, bool, const MethodInfo*);
using TileFrameFn = void (*)(int32_t,int32_t,bool,bool,const MethodInfo*);
using CanKillFn = bool (*)(int32_t, int32_t, bool*, const MethodInfo*);
using LargeDropFn = void (*)(int32_t, int32_t, uint32_t, bool, const MethodInfo*);
DropFn g_origDrop = nullptr;
KillFn g_origKill = nullptr;
DustFn g_origDust = nullptr;
KillSoundFn g_origKillSound=nullptr;
bool g_killSoundInstalled=false;
using PickaxeDamageFn=int32_t (*)(Il2CppObject*,int32_t,int32_t,int32_t,int32_t,uint32_t,const MethodInfo*);
PickaxeDamageFn g_origPickaxeDamage=nullptr;
bool g_miningInstalled=false;
using SaplingAttemptFn=bool (*)(int32_t,int32_t,bool,int32_t,bool,const MethodInfo*);
SaplingAttemptFn g_origSaplingAttempt=nullptr;
const MethodInfo* g_saplingGrow=nullptr;
const MethodInfo* g_saplingLos=nullptr;
const MethodInfo* g_saplingFx=nullptr;
FieldInfo* g_saplingNetMode=nullptr;
bool g_saplingInstalled=false;
FrameFn g_origFrame = nullptr;
TileFrameFn g_originalTileFrame=nullptr;
bool g_tileFrameCallbackInstalled=false;
using WireFn=void (*)(int32_t,int32_t,const MethodInfo*);
using RandomFn=void (*)(int32_t,int32_t,bool,int32_t,const MethodInfo*);
WireFn g_origWire=nullptr;
RandomFn g_origOverground=nullptr,g_origUnderground=nullptr;
using UseFn=void (*)(Il2CppObject*,int32_t,int32_t,const MethodInfo*);
using SlopeFn=bool (*)(int32_t,int32_t,int32_t,bool,bool,const MethodInfo*);
using PoundFn=bool (*)(int32_t,int32_t,const MethodInfo*);
UseFn g_origUse=nullptr;
SlopeFn g_origSlope=nullptr;
PoundFn g_origPound=nullptr;
using PlaceThingFn=void (*)(Il2CppObject*,bool,const MethodInfo*);
using PlaceTileFn=bool (*)(int32_t,int32_t,int32_t,bool,bool,int32_t,int32_t,const MethodInfo*);
PlaceThingFn g_origPlaceThing=nullptr;
PlaceTileFn g_origPlaceTile=nullptr;
UseFn g_origMouseOver=nullptr;
UseFn g_origMouseOverFar=nullptr;
CanKillFn g_origCanKill = nullptr;
const MethodInfo* g_largeDropMethod = nullptr;
FieldInfo* g_solidField = nullptr;
FieldInfo* g_solidTopField = nullptr;
const MethodInfo* g_killMethod = nullptr;
const MethodInfo* g_newDust = nullptr;
using FrameSetter=void (*)(int32_t*,int16_t,const MethodInfo*);
using SkipWireFn=void (*)(int32_t,int32_t,const MethodInfo*);
struct FrameToggleBinding {
    FrameSetter setX=nullptr,setY=nullptr;
    SkipWireFn skip=nullptr;
    const MethodInfo *x=nullptr,*y=nullptr,*skipMethod=nullptr;
    FieldInfo *running=nullptr,*netMode=nullptr;
    bool ready=false;
};
FrameToggleBinding g_frameToggle{};

bool nameOf(const Il2CppType* type, const char* expected) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* value = a.type_get_name(type);
    const bool ok = value && std::strcmp(value, expected) == 0;
    if (value) a.il2cpp_free(value);
    return ok;
}

bool safeTile(const MethodInfo* method, unsigned argument, unsigned total) {
    auto& a = il2cpp::api();
    if (!method || !a.method_get_param_count || !a.class_value_size ||
        a.method_get_param_count(method) != total ||
        !nameOf(a.method_get_param(method, argument), "Terraria.Tile"))
        return false;
    Il2CppClass* tile = a.class_from_il2cpp_type(a.method_get_param(method, argument));
    if (!tile) return false;
    uint32_t align = 0;
    return a.class_value_size(tile, &align) == 4;
}

bool bindTileStyleLookup() {
    if(g_tileStyleMethod)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||!a.method_get_return_type||
       !a.method_is_instance||!a.class_from_il2cpp_type||
       !a.class_is_valuetype||!a.class_value_size||!a.class_instance_size||
       !a.object_get_class||!a.runtime_invoke)return false;
    auto* cls=il2cpp::findClass({"Terraria.ObjectData","TileObjectData",{}});
    const auto* method=cls?a.class_get_method_from_name(cls,"GetTileStyle",1):nullptr;
    if(!method||a.method_is_instance(method)||!safeTile(method,0,1)||
       !nameOf(a.method_get_return_type(method),"System.Int32")||
       !il2cpp::methodPointer(method))return false;
    auto* resultClass=a.class_from_il2cpp_type(a.method_get_return_type(method));
    uint32_t alignment=0;
    if(!resultClass||!a.class_is_valuetype(resultClass)||
       a.class_value_size(resultClass,&alignment)!=static_cast<int>(sizeof(int32_t))||
       a.class_instance_size(resultClass)<sizeof(Il2CppObject)+sizeof(int32_t))
        return false;
    g_tileStyleReturnClass=resultClass;
    g_tileStyleMethod=method;
    return true;
}
int readNativeTileStyle(uint32_t tile) {
    if(!g_tileStyleMethod||!g_tileStyleReturnClass)return -1;
    auto& a=il2cpp::api();
    int32_t offset=static_cast<int32_t>(tile);
    void* arguments[]={&offset};
    Il2CppObject* exception=nullptr;
    Il2CppObject* result=a.runtime_invoke(g_tileStyleMethod,nullptr,arguments,&exception);
    if(exception||!result||a.object_get_class(result)!=g_tileStyleReturnClass)return -1;
    int32_t style=-1;
    std::memcpy(&style,reinterpret_cast<const char*>(result)+sizeof(Il2CppObject),sizeof(style));
    return style>=0&&style<=4095?style:-1;
}

bool supports(int x, int y, const runtime::TileArrays& world) {
    if (x < 0 || y < 0 || x >= world.width || y >= world.height) return false;
    const int type = runtime::tileTypeAt(x, y);
    if (type < 0) return false;
    if (!g_solidField || !g_solidTopField) return true;
    Il2CppArray *g_solid = nullptr, *g_solidTop = nullptr;
    auto& a = il2cpp::api();
    a.field_static_get_value(g_solidField, &g_solid);
    a.field_static_get_value(g_solidTopField, &g_solidTop);
    if (!g_solid || !g_solidTop || static_cast<uintptr_t>(type) >= g_solid->length ||
        static_cast<uintptr_t>(type) >= g_solidTop->length) return true;
    const auto* solid = static_cast<const uint8_t*>(arrayData(g_solid));
    const auto* top = static_cast<const uint8_t*>(arrayData(g_solidTop));
    return solid[type] != 0 || top[type] != 0;
}

bool cellIs(int x, int y, int type, const runtime::TileArrays& world) {
    return x >= 0 && y >= 0 && x < world.width && y < world.height &&
           runtime::tileTypeAt(x, y) == type;
}

std::map<std::pair<int,int>,uint64_t> g_tileCallbackFailures;
void reportTileCallbackFailure(const NativeTileGameplaySpec& spec,int bit,const std::string& actual) {
    const auto count=++g_tileCallbackFailures[{spec.type,bit}];
    if(count>3&&(count&(count-1))!=0)return;
    BL_WARN("[CSharp][WARN][TileCallback] Mod=%s Class=%s Assembly=%s ContentId=%d Callback=%d Stage=Invoke Result=NativeFallback FailureCount=%llu Actual=%s",
        spec.mod.c_str(),spec.name.c_str(),spec.assembly.c_str(),spec.type,bit,
        static_cast<unsigned long long>(count),actual.c_str());
}
bool managedTileReady(const NativeTileGameplaySpec& spec,int bit) {
    const int selected = bit == 13 ? (spec.callbackMask & tile_policy::kManagedDrawCallbacks) :
                         (spec.callbackMask & (1 << bit));
    if(!selected||spec.assembly.empty())return false;
    std::string error;
    const int state=managedTileWarmupState(spec.mod,spec.assembly,spec.name,&error);
    if(state<0)reportTileCallbackFailure(spec,bit,error);
    return state==1;
}
bool invokeTileBool(const NativeTileGameplaySpec& spec,int bit,int x,int y,bool* out) {
    if(!out||!managedTileReady(spec,bit))return false;
    std::string reply;
    if(!invokeManagedTile(spec.mod,spec.assembly,spec.name,bit,x,y,&reply)){reportTileCallbackFailure(spec,bit,reply);return false;}
    if(reply=="1"){*out=true;return true;}
    if(reply=="0"){*out=false;return true;}
    reportTileCallbackFailure(spec,bit,"Expected=BooleanWire Actual="+reply);return false;
}
void dispatchTileEvent(const NativeTileGameplaySpec& spec,int bit,int x,int y) {
    if(managedTileReady(spec,bit)&&!queueManagedTileEvent(spec.mod,spec.assembly,spec.name,bit,x,y))
        reportTileCallbackFailure(spec,bit,"Reason=EventSubmissionRejected");
}
bool applyManagedTileDraw(int x,int y,int type,runtime::TileDrawData* data,bool special) {
    if(!data || type < runtime::kVanillaTileCount || type >= 65536)return false;
    const auto binding=g_managedDrawBindings.load(type);
    if(!binding || !managedTileReady(binding->spec,13))return false;
    const auto& spec=binding->spec;
    int frameIndex=-1;
    if(spec.animationFrames>1)
        frameIndex=std::clamp(g_animatedDrawFrames[type].load(std::memory_order_relaxed),
                              0,spec.animationFrames-1);
    char snapshot[192]{};
    const int length=std::snprintf(snapshot,sizeof(snapshot),"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        x,y,data->frameX,data->frameY,data->width,data->height,
        data->top,data->addFrX,data->addFrY,data->effects,special?1:0,type,frameIndex);
    if(length<=0||static_cast<size_t>(length)>=sizeof(snapshot))return false;
    std::string reply;
    if(!invokeManagedTileDraw(spec.mod,spec.assembly,spec.name,
       std::string(snapshot,static_cast<size_t>(length)),&reply,1200)) {
        reportTileCallbackFailure(spec,13,reply);
        return false;
    }
    tile_draw::Data draw{};
    tile_sprite::Batch sprites{};
    tile_particle::Batch particles{};
    bool hasSprites=false,hasParticles=false;
    if(!tile_particle::parseReply(reply,&draw,&sprites,&hasSprites,
                                  &particles,&hasParticles)) {
        reportTileCallbackFailure(spec,13,"Stage=DecodeDraw Reason=InvalidReply");
        return false;
    }
    if(spec.frameToggle==1 && spec.animationFrames==8 &&
       spec.animationFrameHeight==36)
        draw.addFrameY=tile_animation::campfireFrameOffset(
            draw.frameY,frameIndex,draw.addFrameY);
    if(hasParticles&&(special||!tile_particle::owned(particles,type,x,y))) {
        reportTileCallbackFailure(spec,13,"Stage=ValidateParticles Reason=OwnershipOrPass");
        return false;
    }
    if(hasSprites) {
        if(special||!tile_sprite::ownTileSprites(sprites,type,x,y)||
           !tile_sprite::stageNativeTileSpritesForCurrentFrame(sprites)) {
            reportTileCallbackFailure(spec,13,"Stage=QueueSprites Reason=InvalidOrUnavailable");
            return false;
        }
    }
    if(hasParticles&&!tile_sprite::stageNativeTileParticlesForCurrentFrame(particles))
        reportTileCallbackFailure(spec,13,"Stage=QueueDust Reason=FrameBudgetOrUnavailable");
    data->frameX=draw.frameX;
    data->frameY=draw.frameY;
    data->width=draw.width;
    data->height=draw.height;
    data->top=draw.top;
    data->addFrX=draw.addFrameX;
    data->addFrY=draw.addFrameY;
    data->effects=draw.effects;
    if (!draw.drawDefault) { data->width=0; data->height=0; }
    return true;
}
void hkGetItemDrops(int32_t x, int32_t y, uint32_t tile,
                    int32_t* drop, int32_t* stack, int32_t* extra, int32_t* extraStack,
                    bool* noPrefix, bool large, const MethodInfo* method) {
    if (g_origDrop) g_origDrop(x, y, tile, drop, stack, extra, extraStack, noPrefix, large, method);
    if (!g_dropInstalled || !drop || !stack || !extra || !extraStack) return;
    const auto it = g_tiles.find(runtime::tileTypeAt(x, y));
    if (it == g_tiles.end()) return;
    const auto& behavior = it->second;
    if (!tile_policy::shouldDrop(behavior.spec.shape, large, behavior.spec.allowDrops)) {
        *drop = 0; *stack = 0; *extra = 0; *extraStack = 0;
        return;
    }
    bool allowDrop=true;
    if(invokeTileBool(behavior.spec,1,x,y,&allowDrop)&&!allowDrop) {
        *drop=0;*stack=0;*extra=0;*extraStack=0;return;
    }
    if((behavior.spec.callbackMask&(1<<27))&&managedTileReady(behavior.spec,27)) {
        std::string reply;
        tile_drop::Drops selected{};
        if(invokeManagedTileDrops(behavior.spec.mod,behavior.spec.assembly,
             behavior.spec.name,x,y,&reply,900)&&
           tile_drop::parseReply(reply,&selected)) {
            if(selected.custom) {
                *drop=selected.first.type;*stack=selected.first.stack;
                *extra=selected.second.type;*extraStack=selected.second.stack;
                return;
            }
        } else reportTileCallbackFailure(behavior.spec,27,reply);
    }
    const int style=behavior.spec.shape==0?0:readNativeTileStyle(tile);
    if(!behavior.styleDrops.empty()) {
        auto registered=style>=0?behavior.styleDrops.find(style):behavior.styleDrops.end();
        if(registered==behavior.styleDrops.end())
            registered=behavior.styleDrops.find(-1);
        if(registered!=behavior.styleDrops.end()) {
            *drop=registered->second;*stack=1;*extra=0;*extraStack=0;
            return;
        }
        if(style<0)return;
    }
    const std::string key=behavior.spec.mod + "/" + behavior.spec.name;
    int itemType=style>=0?basicItemForTileStyle(key,style):behavior.itemType;
    if(itemType<=0)itemType=basicItemForTile(key);
    if (itemType <= 0) return;
    *drop = itemType;
    *stack = 1;
    *extra = 0;
    *extraStack = 0;
}

void hkKillTile(int32_t x, int32_t y, bool fail, bool effectOnly,
                bool noItem, const MethodInfo* method) {
    if (!g_origKill) return;
    const auto it = g_tiles.find(runtime::tileTypeAt(x, y));
    if (it == g_tiles.end()) {
        g_origKill(x, y, fail, effectOnly, noItem, method);
        return;
    }
    if((it->second.spec.callbackMask&(1<<22))&&managedTileReady(it->second.spec,22)) {
        std::string response;
        bool nextFail=fail,nextEffectOnly=effectOnly,nextNoItem=noItem;
        if(invokeManagedTileKill(it->second.spec.mod,it->second.spec.assembly,
              it->second.spec.name,x,y,fail,effectOnly,noItem,&response,1100)&&
           tile_kill::parseReply(response,&nextFail,&nextEffectOnly,&nextNoItem)) {
            fail=nextFail;effectOnly=nextEffectOnly;noItem=nextNoItem;
        } else reportTileCallbackFailure(it->second.spec,22,response);
    }
    const int lastOwner = g_dustOwner, lastBudget = g_dustBudget;
    g_dustOwner = it->first;
    g_dustBudget = tile_policy::dustBudget(fail, it->second.spec.failDust,
                                           it->second.spec.normalDust);
    if((it->second.spec.callbackMask&(1<<24))&&managedTileReady(it->second.spec,24)) {
        std::string reply;
        int value=g_dustBudget;
        if(invokeManagedTileDustCount(it->second.spec.mod,it->second.spec.assembly,
             it->second.spec.name,x,y,fail,value,&reply,700)&&
           tile_dust::parseCount(reply,&value))g_dustBudget=value;
        else reportTileCallbackFailure(it->second.spec,24,reply);
    }
    g_origKill(x, y, fail, effectOnly, noItem, method);
    g_dustOwner = lastOwner;
    g_dustBudget = lastBudget;
}

int32_t spawnDust(int32_t x, int32_t y, int32_t id) {
    auto& a = il2cpp::api();
    if (!g_newDust || id < 0) return -1;
    const float position[2] = {static_cast<float>(x * 16), static_cast<float>(y * 16)};
    const int32_t width = 16, height = 16, alpha = 0;
    const float speedX = 0.f, speedY = 0.f, scale = 1.f;
    const uint32_t color = 0xffffffffu;
    void* args[] = {
        const_cast<float*>(position), const_cast<int32_t*>(&width),
        const_cast<int32_t*>(&height), &id, const_cast<float*>(&speedX),
        const_cast<float*>(&speedY), const_cast<int32_t*>(&alpha),
        const_cast<uint32_t*>(&color), const_cast<float*>(&scale)
    };
    Il2CppObject* exc = nullptr;
    Il2CppObject* result = a.runtime_invoke(g_newDust, nullptr, args, &exc);
    if (!result || exc) return -1;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(result) + sizeof(Il2CppObject));
}

int32_t hkMakeDust(int32_t x, int32_t y, uint32_t tile, const MethodInfo* method) {
    const auto it = g_tiles.find(runtime::tileTypeAt(x, y));
    if (it == g_tiles.end() || !g_dustInstalled)
        return g_origDust ? g_origDust(x, y, tile, method) : 6000;
    if (g_dustOwner == it->first) {
        if (g_dustBudget == 0) return 6000;
        if (g_dustBudget > 0) --g_dustBudget;
    }
    int dustId=it->second.dustType;
    if((it->second.spec.callbackMask&(1<<25))&&managedTileReady(it->second.spec,25)) {
        std::string reply;
        int nextId=dustId;
        bool create=true;
        if(invokeManagedTileCreateDust(it->second.spec.mod,it->second.spec.assembly,
             it->second.spec.name,x,y,dustId,&reply,450)&&
           tile_dust::parseCreate(reply,&create,&nextId)) {
            if(!create||nextId<0)return 6000;
            dustId=nextId;
        } else reportTileCallbackFailure(it->second.spec,25,reply);
    }
    const int dust = spawnDust(x, y, dustId);
    return dust >= 0 ? dust : (g_origDust ? g_origDust(x, y, tile, method) : 6000);
}

void hkTileFrame(int32_t x, int32_t y, int32_t type, uint32_t tile,
                 bool reset, const MethodInfo* method) {
    const auto it = g_tiles.find(type);
    const auto shape=it==g_tiles.end()?tile_policy::TileFootprint{}:
        tile_policy::footprint(it->second.spec.shape);
    if(!g_frameInstalled||shape.width==0||g_frameCleanup) {
        if(g_origFrame)g_origFrame(x,y,type,tile,reset,method);
        return;
    }
    runtime::TileArrays world{};
    if(!runtime::tileArrays(&world)||x<0||y<0||
       x>=world.width||y>=world.height||!world.lookup||
       !world.frameX||!world.frameY) {
        if(g_origFrame)g_origFrame(x,y,type,tile,reset,method);
        return;
    }
    const size_t index=static_cast<size_t>(y)*world.width+x;
    const int frameX=world.frameX[world.lookup[index]];
    const int frameY=world.frameY[world.lookup[index]];
    int left=0,top=0;
    if(!tile_policy::origin(it->second.spec.shape,x,y,frameX,frameY,&left,&top)||
       left+shape.width>world.width||top+shape.height>world.height) {
        if(g_origFrame)g_origFrame(x,y,type,tile,reset,method);
        return;
    }
    std::array<bool,9> cells{};
    bool intact=true;
    int firstX=-1,firstY=-1;
    for(int dy=0;dy<shape.height;++dy) {
        for(int dx=0;dx<shape.width;++dx) {
            const int px=left+dx,py=top+dy;
            const size_t lookupIndex=static_cast<size_t>(py)*world.width+px;
            const bool occupied=cellIs(px,py,type,world);
            const bool matched=occupied&&tile_policy::frameMatches(
                it->second.spec.shape,world.frameX[world.lookup[lookupIndex]],
                world.frameY[world.lookup[lookupIndex]],frameX,frameY,dx,dy);
            cells[static_cast<size_t>(dy)*shape.width+dx]=matched;
            if(matched&&firstX<0){firstX=px;firstY=py;}
            if(!matched)intact=false;
        }
    }
    for(int dx=0;dx<shape.width;++dx)
        if(!supports(left+dx,top+shape.height,world))intact=false;
    if(intact)return;
    if(firstX<0)return;
    g_frameCleanup=true;
    auto& a=il2cpp::api();
    if(g_largeDropMethod) {
        int32_t dropX=firstX,dropY=firstY;
        uint32_t sourceTile=static_cast<uint32_t>(firstY*world.width+firstX);
        bool large=true;
        void* args[]={&dropX,&dropY,&sourceTile,&large};
        Il2CppObject* exc=nullptr;
        a.runtime_invoke(g_largeDropMethod,nullptr,args,&exc);
        if(exc)BL_ERROR("[CSharp][Tile] Drop de objeto composto falhou shape=%d",
                         it->second.spec.shape);
    }
    if(g_killMethod) {
        for(int dy=0;dy<shape.height;++dy)for(int dx=0;dx<shape.width;++dx) {
            if(!cells[static_cast<size_t>(dy)*shape.width+dx])continue;
            int32_t killX=left+dx,killY=top+dy;
            bool fail=false,effectOnly=false,noItem=true;
            void* args[]={&killX,&killY,&fail,&effectOnly,&noItem};
            Il2CppObject* exc=nullptr;
            a.runtime_invoke(g_killMethod,nullptr,args,&exc);
            if(exc)BL_ERROR("[CSharp][Tile] Remocao de objeto composto falhou shape=%d",
                             it->second.spec.shape);
        }
    }
    g_frameCleanup=false;
    if((it->second.spec.callbackMask&(1<<23))&&managedTileReady(it->second.spec,23)) {
        const int objectFrameX=frameX-(frameX%shape.frameWidth);
        const int objectFrameY=frameY-(frameY%shape.frameHeight);
        if(!queueManagedTileMultiKill(it->second.spec.mod,it->second.spec.assembly,
             it->second.spec.name,left,top,objectFrameX,objectFrameY))
            reportTileCallbackFailure(it->second.spec,23,"Reason=MultiTileEventQueueRejected");
    }
}

int resolveDustId(const std::string& name) {
    if (name.empty()) return -1;
    auto& a = il2cpp::api();
    Il2CppClass* dust = il2cpp::findClass({"Terraria.ID", "DustID", {}});
    FieldInfo* f = dust ? il2cpp::findField(dust, name) : nullptr;
    if (!f || !a.field_get_type || !a.field_get_flags) return -1;
    const bool small = nameOf(a.field_get_type(f), "System.Int16");
    const bool integer = nameOf(a.field_get_type(f), "System.Int32");
    if (!small && !integer) return -1;
    int32_t value = -1;
    if ((a.field_get_flags(f) & 0x40u) != 0u) {
        using DefaultFn = const void* (*)(const FieldInfo*, const Il2CppType**);
        auto fn = reinterpret_cast<DefaultFn>(dlsym(RTLD_DEFAULT, "il2cpp_field_get_default_value"));
        const Il2CppType* type = nullptr;
        const void* data = fn ? fn(f, &type) : nullptr;
        if (!data) return -1;
        if (small) {
            int16_t shortValue = -1;
            std::memcpy(&shortValue, data, sizeof(shortValue));
            value = shortValue;
        } else {
            std::memcpy(&value, data, sizeof(value));
        }
    } else if (small) {
        int16_t shortValue = -1;
        a.field_static_get_value(f, &shortValue);
        value = shortValue;
    } else {
        a.field_static_get_value(f, &value);
    }
    return value > 0 && value < 2000 ? value : -1;
}



bool setupTileWorldScan(bool lighting,bool nearby) {
    auto& a=il2cpp::api();
    if(!a.field_static_get_value)return false;
    if(!g_tileScanReady) {
        auto* main=il2cpp::findClass({"Terraria","Main",{}});
        std::string error;
        if(!il2cpp::nativeFieldContract(main,"screenPosition","Microsoft.Xna.Framework.Vector2",
                   true,8,&g_screenPositionField,nullptr,&error)||
           !il2cpp::nativeFieldContract(main,"screenWidth","System.Int32",
                   true,4,&g_screenWidthField,nullptr,&error)||
           !il2cpp::nativeFieldContract(main,"screenHeight","System.Int32",
                   true,4,&g_screenHeightField,nullptr,&error)||
           (nearby&&(!il2cpp::nativeFieldContract(main,"buffScanAreaWidth","System.Int32",
                   true,4,&g_buffScanWidthField,nullptr,&error)||
           !il2cpp::nativeFieldContract(main,"buffScanAreaHeight","System.Int32",
                   true,4,&g_buffScanHeightField,nullptr,&error)))) {
            BL_ERROR("[CSharp][TileScan] Native field contract mismatch: %s",error.c_str());
            return false;
        }
        g_tileScanReady=true;
    }
    if(nearby&&(!g_buffScanWidthField||!g_buffScanHeightField)) {
        auto* main=il2cpp::findClass({"Terraria","Main",{}});
        std::string error;
        if(!il2cpp::nativeFieldContract(main,"buffScanAreaWidth","System.Int32",
                 true,4,&g_buffScanWidthField,nullptr,&error)||
           !il2cpp::nativeFieldContract(main,"buffScanAreaHeight","System.Int32",
                 true,4,&g_buffScanHeightField,nullptr,&error))return false;
    }
    if(lighting&&!g_tileLightReady) {
        if(!a.method_get_param_count||!a.method_get_param||!a.method_get_return_type||
           !a.runtime_invoke)return false;
        auto* light=il2cpp::findClass({"Terraria","Lighting",{}});
        const auto* method=light?a.class_get_method_from_name(light,"AddLight",5):nullptr;
        if(!method||a.method_get_param_count(method)!=5||
           !nameOf(a.method_get_return_type(method),"System.Void")||
           !nameOf(a.method_get_param(method,0),"System.Int32")||
           !nameOf(a.method_get_param(method,1),"System.Int32")||
           !nameOf(a.method_get_param(method,2),"System.Single")||
           !nameOf(a.method_get_param(method,3),"System.Single")||
           !nameOf(a.method_get_param(method,4),"System.Single")||
           !il2cpp::methodPointer(method))return false;
        g_addTileLight=method;
        g_tileLightReady=true;
    }
    return true;
}
struct TileRect {int left=0,top=0,width=0,height=0;};
TileRect tileScreenRect(const runtime::TileArrays& world,bool buff) {
    auto& a=il2cpp::api();
    struct Vec {float x=0,y=0;} screen;
    int32_t width=0,height=0;
    a.field_static_get_value(g_screenPositionField,&screen);
    a.field_static_get_value(g_screenWidthField,&width);
    a.field_static_get_value(g_screenHeightField,&height);
    if(!std::isfinite(screen.x)||!std::isfinite(screen.y)||
       width<1||width>8192||height<1||height>8192)return {};
    int x=static_cast<int>(std::floor(screen.x/16.0f));
    int y=static_cast<int>(std::floor(screen.y/16.0f));
    int cols=(width+15)/16,rows=(height+15)/16;
    if(buff) {
        int32_t buffWidth=0,buffHeight=0;
        a.field_static_get_value(g_buffScanWidthField,&buffWidth);
        a.field_static_get_value(g_buffScanHeightField,&buffHeight);
        if(buffWidth<1||buffWidth>256||buffHeight<1||buffHeight>256)return {};
        x+=cols/2-buffWidth/2;
        y+=rows/2-buffHeight/2;
        cols=buffWidth;rows=buffHeight;
    } else {
        x-=28;y-=28;cols+=56;rows+=56;
    }
    const int left=std::clamp(x,0,world.width);
    const int top=std::clamp(y,0,world.height);
    const int right=std::clamp(x+cols,0,world.width);
    const int bottom=std::clamp(y+rows,0,world.height);
    return {left,top,std::max(0,right-left),std::max(0,bottom-top)};
}
void scanTileNearby(const runtime::TileArrays& world,bool visual) {
    const TileRect region=tileScreenRect(world,!visual);
    const size_t total=static_cast<size_t>(region.width)*region.height;
    if(!total)return;
    size_t& cursor=visual?g_tileScanVisualCursor:g_tileScanBuffCursor;
    cursor%=total;
    const size_t first=cursor;
    int sent=0;
    size_t visited=0;
    const size_t budget=tile_scan::scanBudget(total);
    for(;visited<budget&&sent<6;++visited) {
        const size_t index=(first+visited)%total;
        const int x=region.left+static_cast<int>(index%region.width);
        const int y=region.top+static_cast<int>(index/region.width);
        const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
        if(it==g_tiles.end()||!managedTileReady(it->second.spec,11))continue;
        const auto key=tile_scan::cellKey(x,y,visual);
        const auto previous=g_tileNearbyEvents.find(key);
        if(previous!=g_tileNearbyEvents.end()&&g_tileScanTick-previous->second<6)continue;
        if(!queueManagedTileNearby(it->second.spec.mod,it->second.spec.assembly,
              it->second.spec.name,x,y,visual))break;
        g_tileNearbyEvents[key]=g_tileScanTick;
        ++sent;
    }
    cursor=tile_scan::nextCursor(first,visited,total);
    if(g_tileNearbyEvents.size()>2048)g_tileNearbyEvents.clear();
}
bool nativeTileLit(const runtime::TileArrays& world,int x,int y,int toggle) {
    if(!toggle)return true;
    if(!world.lookup||!world.frameX||!world.frameY||
       x<0||y<0||x>=world.width||y>=world.height)return false;
    const uint32_t id=world.lookup[static_cast<size_t>(y)*world.width+x];
    return tile_toggle::lit(toggle,world.frameX[id],world.frameY[id]);
}
void scanTileLighting(const runtime::TileArrays& world) {
    const TileRect region=tileScreenRect(world,false);
    const size_t total=static_cast<size_t>(region.width)*region.height;
    if(!total||!g_addTileLight)return;
    g_tileScanLightCursor%=total;
    const size_t first=g_tileScanLightCursor;
    int evaluated=0;
    size_t visited=0;
    const size_t budget=tile_scan::scanBudget(total);
    for(;visited<budget&&evaluated<3;++visited) {
        const size_t index=(first+visited)%total;
        const int x=region.left+static_cast<int>(index%region.width);
        const int y=region.top+static_cast<int>(index/region.width);
        const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
        if(it==g_tiles.end()||!managedTileReady(it->second.spec,6))continue;
        const uint64_t key=tile_scan::cellKey(x,y);
        if(!nativeTileLit(world,x,y,it->second.spec.frameToggle)) {
            g_tileLightCache.erase(key);
            continue;
        }
        const auto cached=g_tileLightCache.find(key);
        if(cached!=g_tileLightCache.end()&&g_tileScanTick-cached->second.frame<3)continue;
        evaluated++;
        std::string reply;
        if(invokeManagedTile(it->second.spec.mod,it->second.spec.assembly,
           it->second.spec.name,6,x,y,&reply,600)) {
            int red=0,green=0,blue=0;
            if(tile_scan::parseLight(reply,&red,&green,&blue))
                g_tileLightCache[key]={x,y,it->first,red,green,blue,g_tileScanTick};
        }
    }
    g_tileScanLightCursor=tile_scan::nextCursor(first,visited,total);
    auto& a=il2cpp::api();
    for(auto it=g_tileLightCache.begin();it!=g_tileLightCache.end();) {
        auto& entry=it->second;
        if(g_tileScanTick-entry.frame>180||entry.x<region.left||
           entry.y<region.top||entry.x>=region.left+region.width||
           entry.y>=region.top+region.height||
           runtime::tileTypeAt(entry.x,entry.y)!=entry.type||
           (g_tiles.find(entry.type)!=g_tiles.end()&&
            !nativeTileLit(world,entry.x,entry.y,g_tiles.at(entry.type).spec.frameToggle))) {
            it=g_tileLightCache.erase(it);continue;
        }
        if(entry.r||entry.g||entry.b) {
            int x=entry.x,y=entry.y;
            float r=entry.r/1000.f,g=entry.g/1000.f,b=entry.b/1000.f;
            void* args[]={&x,&y,&r,&g,&b};
            Il2CppObject* error=nullptr;
            a.runtime_invoke(g_addTileLight,nullptr,args,&error);
        }
        ++it;
    }
}
void scanManagedTileWorld() {
    ++g_tileScanTick;
    if(!g_tileScanReady||g_tiles.empty())return;
    if(!g_usesManagedNearby&&!g_usesManagedLight)return;
    runtime::TileArrays world{};
    if(!runtime::tileArrays(&world))return;
    if(g_usesManagedNearby&&(g_tileScanTick%6==0)) {
        scanTileNearby(world,false);
        scanTileNearby(world,true);
    }
    if(g_usesManagedLight)scanTileLighting(world);
}
void hkAnimateTiles(const MethodInfo* method) {
    if (g_originalAnimate) g_originalAnimate(method);
    scanManagedTileWorld();
    if (g_animatedTiles.empty()) return;
    auto& a = il2cpp::api();
    Il2CppArray* frames = nullptr;
    Il2CppArray* counters = nullptr;
    a.field_static_get_value(g_frameArray, &frames);
    a.field_static_get_value(g_counterArray, &counters);
    if (!il2cpp::nativeArrayContract(frames, "Int32", sizeof(int32_t), 65536) ||
        !il2cpp::nativeArrayContract(counters, "Int32", sizeof(int32_t), 65536)) return;
    auto* frameValues = static_cast<int32_t*>(arrayData(frames));
    auto* counterValues = static_cast<int32_t*>(arrayData(counters));
    int dynamicCallbacks=0;
    for (const auto& [type, policy] : g_animatedTiles) {
        if (type < 0 || static_cast<uintptr_t>(type) >= frames->length ||
            static_cast<uintptr_t>(type) >= counters->length) continue;
        const auto found=g_tiles.find(type);
        if(found!=g_tiles.end() && (found->second.spec.callbackMask & (1<<12)) &&
           dynamicCallbacks < 2 && managedTileReady(found->second.spec,12)) {
            ++dynamicCallbacks;
            std::string reply;
            if(invokeManagedTile(found->second.spec.mod,found->second.spec.assembly,
                found->second.spec.name,12,frameValues[type],counterValues[type],&reply,700)) {
                const auto separator=reply.find(',');
                if(separator!=std::string::npos) {
                    int nextFrame=0,nextCounter=0;
                    const auto first=std::from_chars(reply.data(),reply.data()+separator,nextFrame);
                    const auto second=std::from_chars(reply.data()+separator+1,reply.data()+reply.size(),nextCounter);
                    if(first.ec==std::errc{}&&first.ptr==reply.data()+separator &&
                       second.ec==std::errc{}&&second.ptr==reply.data()+reply.size() &&
                       nextFrame>=0&&nextFrame<=127&&nextCounter>=0&&nextCounter<=600) {
                        frameValues[type]=nextFrame;
                        counterValues[type]=nextCounter;
                        g_animatedDrawFrames[type].store(nextFrame,std::memory_order_relaxed);
                        continue;
                    }
                }
            }
        }
        tile_animation::advance(frameValues[type], counterValues[type], policy);
        g_animatedDrawFrames[type].store(frameValues[type],std::memory_order_relaxed);
    }
}

bool setupNativeTileAnimation() {
    if (g_animationInstalled) return true;
    auto& a = il2cpp::api();
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    std::string error;
    FieldInfo *frames = nullptr, *counters = nullptr;
    if (!il2cpp::nativeFieldContract(main, "tileFrame", "System.Int32[]",
                                     true, sizeof(void*), &frames, nullptr, &error) ||
        !il2cpp::nativeFieldContract(main, "tileFrameCounter", "System.Int32[]",
                                     true, sizeof(void*), &counters, nullptr, &error)) {
        BL_ERROR("[CSharp][TileAnimation] Stage=Bind Reason=%s", error.c_str());
        return false;
    }
    Il2CppArray *frameArray = nullptr, *counterArray = nullptr;
    a.field_static_get_value(frames, &frameArray);
    a.field_static_get_value(counters, &counterArray);
    if (!il2cpp::nativeArrayContract(frameArray, "Int32", sizeof(int32_t), 65536) ||
        !il2cpp::nativeArrayContract(counterArray, "Int32", sizeof(int32_t), 65536)) {
        BL_ERROR("[CSharp][TileAnimation] Stage=Bind Reason=NativeArrayContractMismatch");
        return false;
    }
    const MethodInfo* animate = main ? a.class_get_method_from_name(main, "AnimateTiles", 0) : nullptr;
    if (!animate || !il2cpp::methodPointer(animate) ||
        !hook::install(animate, hkAnimateTiles, &g_originalAnimate)) {
        BL_ERROR("[CSharp][TileAnimation] Stage=Bind Reason=AnimateTilesHookUnavailable");
        return false;
    }
    g_frameArray = frames;
    g_counterArray = counters;
    g_animationInstalled = true;
    return true;
}

bool hkCanKillTile(int32_t x, int32_t y, bool* blockDamaged, const MethodInfo* method) {
    const int type = runtime::tileTypeAt(x, y);
    if (type >= runtime::kVanillaTileCount) {
        const auto it = g_tiles.find(type);
        if(it!=g_tiles.end()) {
            if(tile_policy::blocksCanKillTile(it->second.spec.allowBreaking)) {
                if(blockDamaged)*blockDamaged=false;
                return false;
            }
            bool allowed=true;
            if(invokeTileBool(it->second.spec,0,x,y,&allowed)&&!allowed) {
                if(blockDamaged)*blockDamaged=false;
                return false;
            }
        }
    }
    return g_origCanKill ? g_origCanKill(x, y, blockDamaged, method) : false;
}

bool setupCanKillTileHook() {
    if (g_canKillInstalled) return true;
    auto& a = il2cpp::api();
    if (!a.method_get_param_count || !a.method_get_param || !a.method_get_return_type)
        return false;
    auto* world = il2cpp::findClass({"Terraria", "WorldGen", {}});
    const MethodInfo* method = world ? a.class_get_method_from_name(world, "CanKillTile", 3) : nullptr;
    if (!method || a.method_get_param_count(method) != 3 ||
        !nameOf(a.method_get_param(method, 0), "System.Int32") ||
        !nameOf(a.method_get_param(method, 1), "System.Int32") ||
        !nameOf(a.method_get_param(method, 2), "System.Boolean&") ||
        !nameOf(a.method_get_return_type(method), "System.Boolean") ||
        !il2cpp::methodPointer(method) ||
        !hook::install(method, hkCanKillTile, &g_origCanKill))
        return false;
    g_canKillInstalled = true;
    return true;
}


void hkCampfireSceneScan(Il2CppObject* scene,const void* settings,const MethodInfo* method) {
    if(g_originalSceneScan)g_originalSceneScan(scene,settings,method);
    if(!g_campfireScene.ready||!scene||!settings||g_campfireTiles.empty())return;
    auto* bytes=reinterpret_cast<uint8_t*>(scene);
    if(bytes[g_campfireScene.hasCampfireOffset])return;
    const auto* value=reinterpret_cast<const uint8_t*>(settings);
    float center[2]{};
    std::memcpy(center,value+g_campfireScene.centerOffset,sizeof(center));
    auto& a=il2cpp::api();
    int32_t width=0,height=0;
    a.field_static_get_value(g_campfireScene.scanWidth,&width);
    a.field_static_get_value(g_campfireScene.scanHeight,&height);
    runtime::TileArrays world{};
    if(!runtime::tileArrays(&world)||!world.lookup||!world.type||
       !world.sHeader||!world.frameY)return;
    const auto region=tile_campfire::area(center[0],center[1],width,height,
                                          world.width,world.height);
    for(int y=region.top;y<region.bottom;++y)for(int x=region.left;x<region.right;++x) {
        const uint32_t id=world.lookup[static_cast<size_t>(y)*world.width+x];
        if((world.sHeader[id]&runtime::kTileActiveBit)==0)continue;
        if(g_campfireTiles.find(world.type[id])==g_campfireTiles.end())continue;
        if(tile_campfire::lit(true,world.frameY[id])) {
            bytes[g_campfireScene.hasCampfireOffset]=1;
            return;
        }
    }
}
bool setupNativeCampfireSound() {
    if(g_campfireSound.ready)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||!a.method_get_return_type||
       !a.method_is_instance||!a.class_from_il2cpp_type||!a.class_value_size||
       !a.class_is_valuetype||!a.field_static_get_value||!a.runtime_invoke)return false;
    auto* id=il2cpp::findClass({"Terraria.ID","SoundID",{}});
    auto* engine=il2cpp::findClass({"Terraria.Audio","SoundEngine",{}});
    const auto* play=engine?il2cpp::findMethodBySignature(engine,il2cpp::parseSignature(
        "SoundEffectInstance PlaySound(LegacySoundStyle type, Vector2 position, float pitchOffset, float volumeScale)")):nullptr;
    if(!play||a.method_is_instance(play)||a.method_get_param_count(play)!=4||
       !nameOf(a.method_get_param(play,0),"Terraria.Audio.LegacySoundStyle")||
       !nameOf(a.method_get_param(play,1),"Microsoft.Xna.Framework.Vector2")||
       !nameOf(a.method_get_param(play,2),"System.Single")||
       !nameOf(a.method_get_param(play,3),"System.Single")||
       !il2cpp::methodPointer(play))return false;
    auto* vector=a.class_from_il2cpp_type(a.method_get_param(play,1));
    uint32_t align=0;
    if(!vector||!a.class_is_valuetype(vector)||a.class_value_size(vector,&align)!=8)return false;
    FieldInfo* mech=nullptr;
    std::string error;
    if(!il2cpp::nativeFieldContract(id,"Mech","Terraria.Audio.LegacySoundStyle",
           true,sizeof(void*),&mech,nullptr,&error)) {
        BL_ERROR("[CSharp][CampfireSound] Stage=BindMech Result=Rejected Reason=%s",error.c_str());
        return false;
    }
    if(a.runtime_class_init)a.runtime_class_init(id);
    Il2CppObject* sound=nullptr;
    a.field_static_get_value(mech,&sound);
    if(!sound)return false;
    g_campfireSound={play,mech,true};
    return true;
}
bool playNativeCampfireClick(int x,int y) {
    if(!g_campfireSound.ready)return false;
    auto& a=il2cpp::api();
    Il2CppObject* sound=nullptr;
    a.field_static_get_value(g_campfireSound.mech,&sound);
    if(!sound)return false;
    float position[2]{};
    if(!tile_campfire::soundPosition(x,y,&position[0],&position[1]))return false;
    float pitch=0.0f,volume=1.0f;
    void* args[]={&sound,position,&pitch,&volume};
    Il2CppObject* exception=nullptr;
    a.runtime_invoke(g_campfireSound.play,nullptr,args,&exception);
    return exception==nullptr;
}

bool setupNativeCampfireSceneHook() {
    if(g_campfireScene.ready)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||!a.method_get_return_type||
       !a.method_is_instance||!a.class_from_il2cpp_type||!a.class_is_valuetype||
       !a.class_value_size||!a.field_get_offset||!a.field_static_get_value)return false;
    auto* metrics=il2cpp::findClass({"Terraria","SceneMetrics",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    const auto* method=metrics?a.class_get_method_from_name(metrics,"Scan",1):nullptr;
    if(!method||!a.method_is_instance(method)||a.method_get_param_count(method)!=1||
       !nameOf(a.method_get_return_type(method),"System.Void")||
       !nameOf(a.method_get_param(method,0),"Terraria.SceneMetricsScanSettings")||
       !il2cpp::methodPointer(method))return false;
    auto* settings=a.class_from_il2cpp_type(a.method_get_param(method,0));
    if(!settings)return false;
    const bool value=a.class_is_valuetype(settings);
    std::string error;
    FieldInfo* center=nullptr;
    FieldInfo* hasCampfire=nullptr;
    FieldInfo* scanWidth=nullptr;
    FieldInfo* scanHeight=nullptr;
    int centerOffset=0,hasCampfireOffset=0;
    if(value) {
        uint32_t alignment=0;
        const int size=a.class_value_size(settings,&alignment);
        if(size<=16||!il2cpp::findField(settings,"VisualScanArea"))return false;
        il2cpp::ValueLayout layout{};
        if(!il2cpp::resolveValueLayout(settings,&layout,&error)) {
            BL_ERROR("[CSharp][Campfire] Stage=SceneLayout Result=Rejected Reason=%s",error.c_str());
            return false;
        }
        center=il2cpp::findField(settings,"BiomeScanCenterPositionInWorld");
        size_t offset=0;
        if(!center||!a.field_get_type||!a.field_get_flags||!a.field_get_offset||
           (a.field_get_flags(center)&0x10u)!=0||
           !nameOf(a.field_get_type(center),"Microsoft.Xna.Framework.Vector2")||
           !layout.offset(a.field_get_offset(center),8,&offset)||
           offset>static_cast<size_t>(INT32_MAX))return false;
        centerOffset=static_cast<int>(offset);
    } else if(!il2cpp::nativeFieldContract(settings,"BiomeScanCenterPositionInWorld",
                 "Microsoft.Xna.Framework.Vector2",false,8,&center,&centerOffset,&error)) {
        BL_ERROR("[CSharp][Campfire] Stage=SceneCenter Result=Rejected Reason=%s",error.c_str());
        return false;
    }
    if(!il2cpp::nativeFieldContract(metrics,"HasCampfire",
          "System.Boolean",false,1,&hasCampfire,&hasCampfireOffset,&error)||
       !il2cpp::nativeFieldContract(main,"buffScanAreaWidth",
          "System.Int32",true,4,&scanWidth,nullptr,&error)||
       !il2cpp::nativeFieldContract(main,"buffScanAreaHeight",
          "System.Int32",true,4,&scanHeight,nullptr,&error)) {
        BL_ERROR("[CSharp][Campfire] Stage=SceneMetricsBind Result=Rejected Reason=%s",error.c_str());
        return false;
    }
    if(!hook::install(method,hkCampfireSceneScan,&g_originalSceneScan))return false;
    g_campfireScene={true,centerOffset,hasCampfireOffset,scanWidth,scanHeight};
    return true;
}

bool setupNativeFrameToggle() {
    if(g_frameToggle.ready)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance||
       !a.class_is_valuetype||!a.class_value_size)return false;
    auto* tile=il2cpp::findClass({"Terraria","Tile",{}});
    auto* wiring=il2cpp::findClass({"Terraria","Wiring",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    if(!tile||!wiring||!main)return false;
    uint32_t align=0;
    if(!a.class_is_valuetype(tile)||a.class_value_size(tile,&align)!=4)
        return false;
    const auto validate=[&](const MethodInfo* m,bool instance,const char* type,unsigned count) {
        return m&&il2cpp::methodPointer(m)&&
            bool(a.method_is_instance(m))==instance&&
            a.method_get_param_count(m)==count&&
            nameOf(a.method_get_return_type(m),"System.Void")&&
            (count!=1||nameOf(a.method_get_param(m,0),type));
    };
    FrameToggleBinding binding{};
    binding.x=a.class_get_method_from_name(tile,"set_frameX",1);
    binding.y=a.class_get_method_from_name(tile,"set_frameY",1);
    binding.skipMethod=a.class_get_method_from_name(wiring,"SkipWire",2);
    if(!validate(binding.x,true,"System.Int16",1)||
       !validate(binding.y,true,"System.Int16",1)||
       !validate(binding.skipMethod,false,"",2)||
       !nameOf(a.method_get_param(binding.skipMethod,0),"System.Int32")||
       !nameOf(a.method_get_param(binding.skipMethod,1),"System.Int32"))
        return false;
    std::string error;
    if(!il2cpp::nativeFieldContract(wiring,"running","System.Boolean",true,1,
                                    &binding.running,nullptr,&error)||
       !il2cpp::nativeFieldContract(main,"netMode","System.Int32",true,4,
                                    &binding.netMode,nullptr,&error))return false;
    binding.setX=reinterpret_cast<FrameSetter>(il2cpp::methodPointer(binding.x));
    binding.setY=reinterpret_cast<FrameSetter>(il2cpp::methodPointer(binding.y));
    binding.skip=reinterpret_cast<SkipWireFn>(il2cpp::methodPointer(binding.skipMethod));
    binding.ready=true;
    g_frameToggle=binding;
    return true;
}
bool toggleNativeFrame(const NativeTileGameplaySpec& spec,int x,int y) {
    if(spec.frameToggle==0||!g_frameToggle.ready)return false;
    auto& a=il2cpp::api();
    int32_t netMode=-1;
    a.field_static_get_value(g_frameToggle.netMode,&netMode);
    if(netMode!=0)return false;
    runtime::TileArrays world{};
    if(!runtime::tileArrays(&world)||!world.lookup||!world.frameX||!world.frameY||
       x<0||y<0||x>=world.width||y>=world.height)return false;
    const uint32_t at=world.lookup[static_cast<size_t>(y)*world.width+x];
    tile_toggle::Plan plan{};
    if(!tile_toggle::plan(spec.shape,spec.frameToggle,x,y,
                          world.frameX[at],world.frameY[at],&plan)||
       plan.left+plan.width>world.width||plan.top+plan.height>world.height)
        return false;
    for(int i=0;i<plan.count;++i) {
        const auto& cell=plan.cells[i];
        if(!cellIs(cell.x,cell.y,spec.type,world))return false;
        const uint32_t id=world.lookup[static_cast<size_t>(cell.y)*world.width+cell.x];
        if(world.frameX[id]!=cell.fromX||world.frameY[id]!=cell.fromY)return false;
    }
    const int64_t last=static_cast<int64_t>(plan.top+plan.height-1)*world.width+
                       plan.left+plan.width-1;
    if(last>INT32_MAX)return false;
    bool running=false;
    a.field_static_get_value(g_frameToggle.running,&running);
    for(int i=0;i<plan.count;++i) {
        const auto& cell=plan.cells[i];
        if(running)g_frameToggle.skip(cell.x,cell.y,g_frameToggle.skipMethod);
        int32_t offset=static_cast<int32_t>(static_cast<int64_t>(cell.y)*world.width+cell.x);
        if(spec.frameToggle==1)
            g_frameToggle.setY(&offset,static_cast<int16_t>(cell.toY),g_frameToggle.y);
        else
            g_frameToggle.setX(&offset,static_cast<int16_t>(cell.toX),g_frameToggle.x);
    }
    g_tileLightCache.clear();
    runtime::invalidateManagedTileDrawCache();
    return true;
}
void hkTileWire(int32_t x,int32_t y,const MethodInfo* method) {
    if(g_origWire)g_origWire(x,y,method);
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it!=g_tiles.end()) {
        if(it->second.spec.frameToggle)toggleNativeFrame(it->second.spec,x,y);
        dispatchTileEvent(it->second.spec,2,x,y);
    }
}
void hkOverground(int32_t x,int32_t y,bool npc,int32_t wallDist,const MethodInfo* method) {
    if(g_origOverground)g_origOverground(x,y,npc,wallDist,method);
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it!=g_tiles.end())dispatchTileEvent(it->second.spec,3,x,y);
}
void hkUnderground(int32_t x,int32_t y,bool npc,int32_t wallDist,const MethodInfo* method) {
    if(g_origUnderground)g_origUnderground(x,y,npc,wallDist,method);
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it!=g_tiles.end())dispatchTileEvent(it->second.spec,3,x,y);
}
bool validTileWorldMethod(const MethodInfo* method,int count,bool withRandomArgs) {
    auto& a=il2cpp::api();
    if(!method||!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||a.method_get_param_count(method)!=static_cast<uint32_t>(count)||
       !nameOf(a.method_get_return_type(method),"System.Void")||
       !nameOf(a.method_get_param(method,0),"System.Int32")||
       !nameOf(a.method_get_param(method,1),"System.Int32"))return false;
    if(withRandomArgs&&(!nameOf(a.method_get_param(method,2),"System.Boolean")||
       !nameOf(a.method_get_param(method,3),"System.Int32")))return false;
    return il2cpp::methodPointer(method)!=nullptr;
}
bool setupTileWorldHooks(bool wire,bool random) {
    auto& a=il2cpp::api();
    if(wire&&!g_wireInstalled) {
        auto* cls=il2cpp::findClass({"Terraria","Wiring",{}});
        const MethodInfo* method=cls?a.class_get_method_from_name(cls,"HitWireSingle",2):nullptr;
        if(!validTileWorldMethod(method,2,false)||
           !hook::install(method,hkTileWire,&g_origWire))return false;
        g_wireInstalled=true;
    }
    if(random) {
        auto* cls=il2cpp::findClass({"Terraria","WorldGen",{}});
        if(!g_overgroundInstalled) {
            const MethodInfo* method=cls?a.class_get_method_from_name(cls,"UpdateWorld_OvergroundTile",4):nullptr;
            if(!validTileWorldMethod(method,4,true)||
               !hook::install(method,hkOverground,&g_origOverground))return false;
            g_overgroundInstalled=true;
        }
        if(!g_undergroundInstalled) {
            const MethodInfo* method=cls?a.class_get_method_from_name(cls,"UpdateWorld_UndergroundTile",4):nullptr;
            if(!validTileWorldMethod(method,4,true)||
               !hook::install(method,hkUnderground,&g_origUnderground))return false;
            g_undergroundInstalled=true;
        }
    }
    return true;
}

void hkTileUse(Il2CppObject* player,int32_t x,int32_t y,const MethodInfo* method) {
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(!player||!g_interactInstalled||it==g_tiles.end()||
       ((it->second.spec.callbackMask&(1<<4))==0&&
        !tile_toggle::allowsRightClick(it->second.spec.frameToggle))) {
        if(g_origUse)g_origUse(player,x,y,method);
        return;
    }
    const auto* bytes=reinterpret_cast<const uint8_t*>(player);
    if(!bytes[g_useAttempt]) {
        if(g_origUse)g_origUse(player,x,y,method);
        return;
    }
    uint64_t frame=0;
    il2cpp::api().field_static_get_value(g_gameUpdateField,&frame);
    auto old=g_lastUseFrames.find(player);
    if(old!=g_lastUseFrames.end()&&old->second==frame)return;
    const bool fresh=bytes[g_useRelease]||old==g_lastUseFrames.end()||
        (frame>old->second&&frame-old->second>1);
    g_lastUseFrames[player]=frame;
    if(!fresh)return;
    if(tile_toggle::allowsRightClick(it->second.spec.frameToggle)&&
       toggleNativeFrame(it->second.spec,x,y)) {
        if(it->second.spec.campfire&&!playNativeCampfireClick(x,y))
            BL_WARN("[CSharp][CampfireSound] Stage=Play Result=Unavailable");
        reinterpret_cast<uint8_t*>(player)[g_useHappened]=1;
        return;
    }
    bool handled=false;
    if(invokeTileBool(it->second.spec,4,x,y,&handled)&&handled) {
        reinterpret_cast<uint8_t*>(player)[g_useHappened]=1;
        return;
    }
    if(g_origUse)g_origUse(player,x,y,method);
}

void hkTileSmartInteract(Il2CppObject* provider,const void* settings,const MethodInfo* method) {
    if(g_originalSmartInteract)g_originalSmartInteract(provider,settings,method);
    if(!g_smartInteract.ready||!provider||!settings)return;
    auto& a=il2cpp::api();
    int32_t scan[4]{};
    for(int i=0;i<4;++i)
        std::memcpy(&scan[i],static_cast<const uint8_t*>(settings)+g_smartInteract.settingsOffsets[i],4);
    runtime::TileArrays world{};
    if(!runtime::tileArrays(&world)||!world.lookup||!world.type||!world.sHeader)return;
    tile_smart::Bounds range{};
    if(!tile_smart::bounds(scan[0],scan[1],scan[2],scan[3],world.width,world.height,&range))return;
    auto* list=*reinterpret_cast<Il2CppObject* const*>(
        reinterpret_cast<const uint8_t*>(provider)+g_smartInteract.targetsOffset);
    if(!list)return;
    int customBudget=8;
    for(int y=range.top;y<range.bottom;++y)for(int x=range.left;x<range.right;++x) {
        const uint32_t def=world.lookup[static_cast<size_t>(y)*world.width+x];
        if((world.sHeader[def]&runtime::kTileActiveBit)==0)continue;
        const auto it=g_tiles.find(world.type[def]);
        if(it==g_tiles.end()||
           !tile_smart::eligible(it->second.spec.callbackMask,it->second.spec.frameToggle))
            continue;
        const auto& spec=it->second.spec;
        if((spec.callbackMask&(1<<20))!=0) {
            if(customBudget<=0||!managedTileReady(spec,20))continue;
            --customBudget;
            std::string response;
            if(!invokeManagedTileSmart(spec.mod,spec.assembly,spec.name,x,y,
                 scan[0],scan[1],scan[2],scan[3],&response,450)) {
                reportTileCallbackFailure(spec,20,response);
                continue;
            }
            if(response!="1") {
                if(response!="0")reportTileCallbackFailure(spec,20,"Expected=BooleanWire Actual="+response);
                continue;
            }
        }
        int32_t px=x,py=y;
        void* joinArgs[]={&px,&py};
        Il2CppObject* error=nullptr;
        auto* joined=a.runtime_invoke(g_smartInteract.join,provider,joinArgs,&error);
        if(error||!joined)return;
        const int32_t key=*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(joined)+sizeof(Il2CppObject));
        void* keyArgs[]={const_cast<int32_t*>(&key)};
        auto* exists=a.runtime_invoke(g_smartInteract.contains,list,keyArgs,&error);
        if(error||!exists)return;
        if(*reinterpret_cast<uint8_t*>(exists+1)!=0)continue;
        a.runtime_invoke(g_smartInteract.add,list,keyArgs,&error);
        if(error)return;
    }
}
bool setupTileSmartInteractHook() {
    if(g_smartInteract.ready)return true;
    auto& a=il2cpp::api();
    if(!a.runtime_invoke||!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance||!a.class_from_il2cpp_type||
       !a.class_get_name||!a.class_get_namespace||!a.field_get_flags||
       !a.field_get_type||!a.field_get_offset||!a.class_instance_size)return false;
    auto* provider=il2cpp::findClass({"Terraria.GameContent.ObjectInteractions",
                                     "TileSmartInteractCandidateProvider",{}});
    const auto* scan=provider?a.class_get_method_from_name(provider,"FillPotentialTargetTiles",1):nullptr;
    if(!scan||!a.method_is_instance(scan)||a.method_get_param_count(scan)!=1||
       !nameOf(a.method_get_return_type(scan),"System.Void")||
       !il2cpp::methodPointer(scan))return false;
    auto* settings=a.class_from_il2cpp_type(a.method_get_param(scan,0));
    const char* sn=settings?a.class_get_name(settings):nullptr;
    if(!sn||std::strcmp(sn,"SmartInteractScanSettings")!=0)return false;
    const bool value=a.class_is_valuetype&&a.class_is_valuetype(settings);
    il2cpp::ValueLayout layout{};
    std::string error;
    if(value&&(!il2cpp::resolveValueLayout(settings,&layout,&error)||
               layout.size<=16))return false;
    int offsets[4]{};
    constexpr const char* names[]={"LX","LY","HX","HY"};
    for(int i=0;i<4;++i) {
        FieldInfo* f=nullptr;
        if(value) {
            f=il2cpp::findField(settings,names[i]);
            size_t offset=0;
            if(!f||(a.field_get_flags(f)&0x10u)!=0||
               !nameOf(a.field_get_type(f),"System.Int32")||
               !layout.offset(a.field_get_offset(f),4,&offset)||offset>INT32_MAX)
                return false;
            offsets[i]=static_cast<int>(offset);
        } else if(!il2cpp::nativeFieldContract(settings,names[i],"System.Int32",
                       false,4,&f,&offsets[i],&error))return false;
    }
    FieldInfo* targets=nullptr;
    int targetOffset=-1;
    if(!il2cpp::nativeFieldContract(provider,"targets",
           "System.Collections.Generic.List`1",false,sizeof(void*),
           &targets,&targetOffset,&error))return false;
    auto* listClass=a.class_from_il2cpp_type(a.field_get_type(targets));
    const auto* join=a.class_get_method_from_name(provider,"JoinValue",2);
    const auto* add=listClass?a.class_get_method_from_name(listClass,"Add",1):nullptr;
    const auto* contains=listClass?a.class_get_method_from_name(listClass,"Contains",1):nullptr;
    if(!join||!add||!contains||
       !a.method_is_instance(join)||!a.method_is_instance(add)||!a.method_is_instance(contains)||
       a.method_get_param_count(join)!=2||a.method_get_param_count(add)!=1||
       a.method_get_param_count(contains)!=1||
       !nameOf(a.method_get_param(join,0),"System.Int32")||
       !nameOf(a.method_get_param(join,1),"System.Int32")||
       !nameOf(a.method_get_param(add,0),"System.Int32")||
       !nameOf(a.method_get_param(contains,0),"System.Int32")||
       !nameOf(a.method_get_return_type(join),"System.Int32")||
       !nameOf(a.method_get_return_type(contains),"System.Boolean")||
       !nameOf(a.method_get_return_type(add),"System.Void")||
       !il2cpp::methodPointer(join)||!il2cpp::methodPointer(add)||
       !il2cpp::methodPointer(contains))return false;
    SmartInteractBinding binding{};
    binding.join=join;binding.add=add;binding.contains=contains;
    binding.targetsOffset=targetOffset;
    for(int i=0;i<4;++i)binding.settingsOffsets[i]=offsets[i];
    if(!hook::install(scan,hkTileSmartInteract,&g_originalSmartInteract))return false;
    binding.ready=true;g_smartInteract=binding;
    return true;
}

bool setupTileInteractHook() {
    if(g_interactInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.field_static_get_value||!a.method_get_param||!a.method_get_param_count||
        !a.method_get_return_type)return false;
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    std::string error;
    FieldInfo *attempt=nullptr,*release=nullptr,*happened=nullptr,*frame=nullptr;
    int attemptAt=-1,releaseAt=-1,happenedAt=-1;
    if(!il2cpp::nativeFieldContract(player,"tileInteractAttempted","System.Boolean",
           false,1,&attempt,&attemptAt,&error) ||
       !il2cpp::nativeFieldContract(player,"releaseUseTile","System.Boolean",
           false,1,&release,&releaseAt,&error) ||
       !il2cpp::nativeFieldContract(player,"tileInteractionHappened","System.Boolean",
           false,1,&happened,&happenedAt,&error) ||
       !il2cpp::nativeFieldContract(main,"GameUpdateCount","System.UInt64",
           true,8,&frame,nullptr,&error)) {
        BL_ERROR("[CSharp][TileUse] ABI incompatível: %s",error.c_str());return false;
    }
    const MethodInfo* method=player?a.class_get_method_from_name(player,"TileInteractionsUse",2):nullptr;
    if(!method||a.method_get_param_count(method)!=2||
       !nameOf(a.method_get_param(method,0),"System.Int32")||
       !nameOf(a.method_get_param(method,1),"System.Int32")||
       !nameOf(a.method_get_return_type(method),"System.Void")||
       !il2cpp::methodPointer(method)||!hook::install(method,hkTileUse,&g_origUse))return false;
    g_useAttempt=attemptAt;g_useRelease=releaseAt;
    g_useHappened=happenedAt;g_gameUpdateField=frame;g_interactInstalled=true;
    return true;
}
bool hkSlopeTile(int32_t x,int32_t y,int32_t slope,bool noEffects,bool quiet,
                 const MethodInfo* method) {
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    bool allowed=true;
    if(it!=g_tiles.end()&&invokeTileBool(it->second.spec,8,x,y,&allowed)&&!allowed)return false;
    return g_origSlope?g_origSlope(x,y,slope,noEffects,quiet,method):false;
}
bool hkPoundTile(int32_t x,int32_t y,const MethodInfo* method) {
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    bool allowed=true;
    if(it!=g_tiles.end()&&invokeTileBool(it->second.spec,8,x,y,&allowed)&&!allowed)return false;
    return g_origPound?g_origPound(x,y,method):false;
}
bool setupTileSlopeHooks() {
    if(g_slopeInstalled&&g_poundInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param||!a.method_get_param_count||!a.method_get_return_type)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    const auto* slope=world?a.class_get_method_from_name(world,"SlopeTile",5):nullptr;
    const auto* pound=world?a.class_get_method_from_name(world,"PoundTile",2):nullptr;
    if(!slope||!pound||a.method_get_param_count(slope)!=5||
       a.method_get_param_count(pound)!=2||
       !nameOf(a.method_get_return_type(slope),"System.Boolean")||
       !nameOf(a.method_get_return_type(pound),"System.Boolean")||
       !nameOf(a.method_get_param(slope,0),"System.Int32")||
       !nameOf(a.method_get_param(slope,1),"System.Int32")||
       !nameOf(a.method_get_param(slope,2),"System.Int32")||
       !nameOf(a.method_get_param(slope,3),"System.Boolean")||
       !nameOf(a.method_get_param(slope,4),"System.Boolean")||
       !nameOf(a.method_get_param(pound,0),"System.Int32")||
       !nameOf(a.method_get_param(pound,1),"System.Int32")||
       !il2cpp::methodPointer(slope)||!il2cpp::methodPointer(pound))return false;
    if(!g_slopeInstalled) {
        if(!hook::install(slope,hkSlopeTile,&g_origSlope))return false;
        g_slopeInstalled=true;
    }
    if(!g_poundInstalled) {
        if(!hook::install(pound,hkPoundTile,&g_origPound))return false;
        g_poundInstalled=true;
    }
    return true;
}
void hkPlayerPlaceThing(Il2CppObject* player,bool place,const MethodInfo* method) {
    ++g_playerPlaceDepth;
    if(g_origPlaceThing)g_origPlaceThing(player,place,method);
    --g_playerPlaceDepth;
}
bool hkWorldPlaceTile(int32_t x,int32_t y,int32_t type,bool mute,bool forced,
                      int32_t player,int32_t style,const MethodInfo* method) {
    const bool result=g_origPlaceTile?g_origPlaceTile(x,y,type,mute,forced,player,style,method):false;
    if(!result||g_playerPlaceDepth<=0||type<runtime::kVanillaTileCount)return result;
    const auto it=g_tiles.find(type);
    if(it!=g_tiles.end()&&it->second.spec.shape==0)
        dispatchTileEvent(it->second.spec,7,x,y);
    return result;
}
bool setupTilePlacementHooks() {
    if(g_playerPlacementInstalled&&g_placeTileInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param||!a.method_get_param_count||!a.method_get_return_type)return false;
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    const auto* thing=player?a.class_get_method_from_name(player,"PlaceThing_Tiles",1):nullptr;
    const auto* tile=world?a.class_get_method_from_name(world,"PlaceTile",7):nullptr;
    if(!thing||!tile||a.method_get_param_count(thing)!=1||
       a.method_get_param_count(tile)!=7||
       !nameOf(a.method_get_return_type(thing),"System.Void")||
       !nameOf(a.method_get_return_type(tile),"System.Boolean")||
       !nameOf(a.method_get_param(thing,0),"System.Boolean")||
       !nameOf(a.method_get_param(tile,0),"System.Int32")||
       !nameOf(a.method_get_param(tile,1),"System.Int32")||
       !nameOf(a.method_get_param(tile,2),"System.Int32")||
       !nameOf(a.method_get_param(tile,3),"System.Boolean")||
       !nameOf(a.method_get_param(tile,4),"System.Boolean")||
       !nameOf(a.method_get_param(tile,5),"System.Int32")||
       !nameOf(a.method_get_param(tile,6),"System.Int32")||
       !il2cpp::methodPointer(thing)||!il2cpp::methodPointer(tile))return false;
    if(!g_playerPlacementInstalled) {
        if(!hook::install(thing,hkPlayerPlaceThing,&g_origPlaceThing))return false;
        g_playerPlacementInstalled=true;
    }
    if(!g_placeTileInstalled) {
        if(!hook::install(tile,hkWorldPlaceTile,&g_origPlaceTile))return false;
        g_placeTileInstalled=true;
    }
    return true;
}

void hkTileMouseOver(Il2CppObject* player,int32_t x,int32_t y,const MethodInfo* method) {
    if(g_origMouseOver)g_origMouseOver(player,x,y,method);
    if(!player||!g_mouseOverInstalled)return;
    auto& a=il2cpp::api();
    int32_t targetX=-1,targetY=-1;
    a.field_static_get_value(g_tileTargetX,&targetX);
    a.field_static_get_value(g_tileTargetY,&targetY);
    if(targetX!=x||targetY!=y)return;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end())return;
    if(((it->second.spec.callbackMask&(1<<4))!=0||
        tile_toggle::allowsRightClick(it->second.spec.frameToggle))&&
       g_cursorEnabledOffset>=0&&g_cursorItemOffset>=0&&it->second.itemType>0) {
        auto* pointer=reinterpret_cast<uint8_t*>(player);
        if(!pointer[g_cursorEnabledOffset]) {
            pointer[g_cursorEnabledOffset]=1;
            const int32_t item=it->second.itemType;
            std::memcpy(pointer+g_cursorItemOffset,&item,sizeof(item));
        }
    }
    if((it->second.spec.callbackMask&(1<<9))==0)return;
    uint64_t frame=0;
    a.field_static_get_value(g_mouseFrame,&frame);
    const auto found=g_mouseOverFrames.find(player);
    if(found!=g_mouseOverFrames.end()&&found->second==frame)return;
    g_mouseOverFrames[player]=frame;
    dispatchTileEvent(it->second.spec,9,x,y);
}
void hkTileMouseOverFar(Il2CppObject* player,int32_t x,int32_t y,const MethodInfo* method) {
    if(g_origMouseOverFar)g_origMouseOverFar(player,x,y,method);
    if(!player||!g_mouseOverFarInstalled)return;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end()||(it->second.spec.callbackMask&(1<<10))==0)return;
    uint64_t frame=0;
    il2cpp::api().field_static_get_value(g_mouseFrame,&frame);
    const auto found=g_mouseOverFarFrames.find(player);
    if(found!=g_mouseOverFarFrames.end()&&found->second==frame)return;
    g_mouseOverFarFrames[player]=frame;
    dispatchTileEvent(it->second.spec,10,x,y);
}
bool setupTileMouseOverHooks(bool near,bool far) {
    if((!near||g_mouseOverInstalled)&&(!far||g_mouseOverFarInstalled))return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param||!a.method_get_param_count||
       !a.method_get_return_type||!a.field_static_get_value)return false;
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    std::string error;
    FieldInfo* frame=nullptr;
    if(!il2cpp::nativeFieldContract(main,"GameUpdateCount","System.UInt64",
           true,8,&frame,nullptr,&error))return false;
    FieldInfo* targetX=nullptr;
    FieldInfo* targetY=nullptr;
    FieldInfo* cursorEnabled=nullptr;
    FieldInfo* cursorItem=nullptr;
    int cursorEnabledOffset=-1,cursorItemOffset=-1;
    if(near&&(!il2cpp::nativeFieldContract(player,"tileTargetX","System.Int32",
          true,4,&targetX,nullptr,&error)||
       !il2cpp::nativeFieldContract(player,"tileTargetY","System.Int32",
          true,4,&targetY,nullptr,&error)))return false;
    if(near) {
        if(!il2cpp::nativeFieldContract(player,"cursorItemIconEnabled","System.Boolean",
             false,1,&cursorEnabled,&cursorEnabledOffset,&error)||
           !il2cpp::nativeFieldContract(player,"cursorItemIconID","System.Int32",
             false,4,&cursorItem,&cursorItemOffset,&error)) {
            cursorEnabledOffset=-1;cursorItemOffset=-1;
        }
    }
    const auto resolve=[&](const char* name)->const MethodInfo* {
        const auto* method=player?a.class_get_method_from_name(player,name,2):nullptr;
        if(!method||a.method_get_param_count(method)!=2||
           !nameOf(a.method_get_param(method,0),"System.Int32")||
           !nameOf(a.method_get_param(method,1),"System.Int32")||
           !nameOf(a.method_get_return_type(method),"System.Void")||
           !il2cpp::methodPointer(method))return nullptr;
        return method;
    };
    const auto* nearMethod=near&&!g_mouseOverInstalled?resolve("TileInteractionsMouseOver"):nullptr;
    const auto* farMethod=far&&!g_mouseOverFarInstalled?resolve("TileInteractionsCheckLongDistance"):nullptr;
    if((near&&!g_mouseOverInstalled&&!nearMethod)||(far&&!g_mouseOverFarInstalled&&!farMethod))
        return false;
    if(near&&!g_mouseOverInstalled) {
        if(!hook::install(nearMethod,hkTileMouseOver,&g_origMouseOver))return false;
        g_tileTargetX=targetX;g_tileTargetY=targetY;
        g_cursorEnabledOffset=cursorEnabledOffset;
        g_cursorItemOffset=cursorItemOffset;
        g_mouseOverInstalled=true;
    }
    if(far&&!g_mouseOverFarInstalled) {
        if(!hook::install(farMethod,hkTileMouseOverFar,&g_origMouseOverFar))return false;
        g_mouseOverFarInstalled=true;
    }
    g_mouseFrame=frame;
    return true;
}
void hkTileFrameCallback(int32_t x,int32_t y,bool reset,bool noBreak,const MethodInfo* method) {
    if(!g_originalTileFrame)return;
    const auto it=g_frameCleanup?g_tiles.end():g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end()||(it->second.spec.callbackMask&(1<<21))==0||
       !managedTileReady(it->second.spec,21)) {
        g_originalTileFrame(x,y,reset,noBreak,method);
        return;
    }
    std::string response;
    bool runOriginal=true,nextReset=reset,nextNoBreak=noBreak;
    if(!invokeManagedTileFrame(it->second.spec.mod,it->second.spec.assembly,
         it->second.spec.name,x,y,reset,noBreak,&response,650)||
       !tile_frame::parseReply(response,&runOriginal,&nextReset,&nextNoBreak)) {
        reportTileCallbackFailure(it->second.spec,21,response);
        g_originalTileFrame(x,y,reset,noBreak,method);
        return;
    }
    if(runOriginal)g_originalTileFrame(x,y,nextReset,nextNoBreak,method);
}
bool setupTileFrameCallbackHook() {
    if(g_tileFrameCallbackInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    const auto* frame=world?a.class_get_method_from_name(world,"TileFrame",4):nullptr;
    if(!frame||a.method_is_instance(frame)||a.method_get_param_count(frame)!=4||
       !nameOf(a.method_get_return_type(frame),"System.Void")||
       !nameOf(a.method_get_param(frame,0),"System.Int32")||
       !nameOf(a.method_get_param(frame,1),"System.Int32")||
       !nameOf(a.method_get_param(frame,2),"System.Boolean")||
       !nameOf(a.method_get_param(frame,3),"System.Boolean")||
       !il2cpp::methodPointer(frame)||
       !hook::install(frame,hkTileFrameCallback,&g_originalTileFrame))return false;
    g_tileFrameCallbackInstalled=true;
    return true;
}

void hkTileKillSound(int32_t x,int32_t y,bool fail,uint32_t tile,const MethodInfo* method) {
    if(!g_origKillSound)return;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it!=g_tiles.end()&&(it->second.spec.callbackMask&(1<<26))&&
       managedTileReady(it->second.spec,26)) {
        std::string reply;
        if(invokeManagedTileKillSound(it->second.spec.mod,it->second.spec.assembly,
             it->second.spec.name,x,y,fail,&reply,650)) {
            if(reply=="0")return;
            if(reply=="1") {
                g_origKillSound(x,y,fail,tile,method);
                return;
            }
        }
        reportTileCallbackFailure(it->second.spec,26,reply);
    }
    g_origKillSound(x,y,fail,tile,method);
}
int32_t hkPickaxeDamage(Il2CppObject* player,int32_t x,int32_t y,
                        int32_t pickPower,int32_t hitBufferIndex,
                        uint32_t tile,const MethodInfo* method) {
    const int32_t damage=g_origPickaxeDamage?
        g_origPickaxeDamage(player,x,y,pickPower,hitBufferIndex,tile,method):0;
    if(damage<=0)return damage;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end())return damage;
    return tile_mining::apply(damage,pickPower,it->second.spec.minPick,
                              it->second.spec.mineResist);
}

bool setupNativeTileMiningHook() {
    if(g_miningInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.method_is_instance||!a.method_get_param_count||
       !a.method_get_param||!a.method_get_return_type)return false;
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    const auto* method=player?a.class_get_method_from_name(player,"GetPickaxeDamage",5):nullptr;
    if(!method||!a.method_is_instance(method)||!safeTile(method,4,5)||
       !nameOf(a.method_get_return_type(method),"System.Int32"))return false;
    for(unsigned i=0;i<4;i++)
        if(!nameOf(a.method_get_param(method,i),"System.Int32"))return false;
    if(!il2cpp::methodPointer(method)||
       !hook::install(method,hkPickaxeDamage,&g_origPickaxeDamage))return false;
    g_miningInstalled=true;
    return true;
}

bool saplingInvokeBool(const MethodInfo* method,void** args,bool* value) {
    if(!method||!value)return false;
    auto& a=il2cpp::api();
    Il2CppObject* exc=nullptr;
    Il2CppObject* result=a.runtime_invoke(method,nullptr,args,&exc);
    if(exc||!result)return false;
    *value=*reinterpret_cast<const uint8_t*>(reinterpret_cast<const char*>(result)+sizeof(Il2CppObject))!=0;
    return true;
}

bool hkGrowSapling(int32_t x,int32_t y,bool underground,int32_t addon,
                   bool ignoreWalls,const MethodInfo* method) {
    if(!g_origSaplingAttempt)return false;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end()||(it->second.spec.callbackMask&(1<<28))==0)
        return g_origSaplingAttempt(x,y,underground,addon,ignoreWalls,method);
    const auto& spec=it->second.spec;
    if(!tile_sapling::validInput(x,y,addon))return false;
    auto& a=il2cpp::api();
    int32_t mode=-1;
    if(!g_saplingNetMode||!a.field_static_get_value)return false;
    a.field_static_get_value(g_saplingNetMode,&mode);
    if(mode==1||mode<0||mode>3)return false;
    if(!managedTileReady(spec,28))return false;
    std::string reply;
    bool allow=false;
    if(!invokeManagedTileSapling(spec.mod,spec.assembly,spec.name,x,y,
        underground,addon,ignoreWalls,&reply,850)||
       !tile_sapling::parseReply(reply,&allow)) {
        reportTileCallbackFailure(spec,28,reply);
        return false;
    }
    if(!allow)return false;
    int32_t nativeX=x,nativeY=y,nativeAddon=addon;
    bool nativeWalls=ignoreWalls;
    void* growArgs[]={&nativeX,&nativeY,&nativeAddon,&nativeWalls};
    bool grew=false;
    if(!saplingInvokeBool(g_saplingGrow,growArgs,&grew)||!grew)return false;
    void* fxArgs[]={&nativeX,&nativeY};
    bool playerLos=false;
    if(saplingInvokeBool(g_saplingLos,fxArgs,&playerLos)&&playerLos) {
        Il2CppObject* exc=nullptr;
        a.runtime_invoke(g_saplingFx,nullptr,fxArgs,&exc);
        if(exc)reportTileCallbackFailure(spec,28,"Stage=TreeGrowFXCheck Exception=Native");
    }
    return true;
}

bool setupNativeTileSaplingHook() {
    if(g_saplingInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.runtime_invoke||!a.field_static_get_value||
       !a.method_get_param||!a.method_get_param_count||
       !a.method_get_return_type||!a.method_is_instance||
       !a.class_from_il2cpp_type||!a.class_is_valuetype||
       !a.class_value_size)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    if(!world||!main)return false;
    const auto* attempt=a.class_get_method_from_name(world,"AttemptToGrowTreeFromSapling",5);
    const auto* grow=a.class_get_method_from_name(world,"GrowTree",4);
    const auto* los=a.class_get_method_from_name(world,"PlayerLOS",2);
    const auto* fx=a.class_get_method_from_name(world,"TreeGrowFXCheck",2);
    const auto signature=[&](const MethodInfo* m,std::initializer_list<const char*> args,
                             const char* result)->bool {
        if(!m||a.method_is_instance(m)||a.method_get_param_count(m)!=args.size()||
           !nameOf(a.method_get_return_type(m),result)||!il2cpp::methodPointer(m))
           return false;
        unsigned i=0;
        for(const char* arg:args)
            if(!nameOf(a.method_get_param(m,i++),arg))return false;
        return true;
    };
    if(!signature(attempt,{"System.Int32","System.Int32","System.Boolean",
                          "System.Int32","System.Boolean"},"System.Boolean")||
       !signature(grow,{"System.Int32","System.Int32","System.Int32",
                        "System.Boolean"},"System.Boolean")||
       !signature(los,{"System.Int32","System.Int32"},"System.Boolean")||
       !signature(fx,{"System.Int32","System.Int32"},"System.Void"))
       return false;
    std::string error;
    FieldInfo* net=nullptr;
    if(!il2cpp::nativeFieldContract(main,"netMode","System.Int32",true,4,
                                     &net,nullptr,&error))return false;
    for(const auto* method:{attempt,grow,los}) {
        auto* cls=a.class_from_il2cpp_type(a.method_get_return_type(method));
        uint32_t alignment=0;
        if(!cls||!a.class_is_valuetype(cls)||a.class_value_size(cls,&alignment)!=1)return false;
    }
    g_saplingGrow=grow;g_saplingLos=los;g_saplingFx=fx;g_saplingNetMode=net;
    if(!hook::install(attempt,hkGrowSapling,&g_origSaplingAttempt))return false;
    g_saplingInstalled=true;
    return true;
}

bool setupNativeTileKillSoundHook() {
    if(g_killSoundInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    const auto* sound=world?a.class_get_method_from_name(world,"KillTile_PlaySounds",4):nullptr;
    if(!sound||a.method_is_instance(sound)||!safeTile(sound,3,4)||
       !nameOf(a.method_get_param(sound,0),"System.Int32")||
       !nameOf(a.method_get_param(sound,1),"System.Int32")||
       !nameOf(a.method_get_param(sound,2),"System.Boolean")||
       !nameOf(a.method_get_return_type(sound),"System.Void")||
       !il2cpp::methodPointer(sound)||
       !hook::install(sound,hkTileKillSound,&g_origKillSound))return false;
    g_killSoundInstalled=true;
    return true;
}

bool setupHooks() {
    if (g_installed) return true;
    auto& a = il2cpp::api();
    Il2CppClass* world = il2cpp::findClass({"Terraria", "WorldGen", {}});
    const MethodInfo* drop = world ? a.class_get_method_from_name(world, "KillTile_GetItemDrops", 9) : nullptr;
    const MethodInfo* dust = world ? a.class_get_method_from_name(world, "KillTile_MakeTileDust", 3) : nullptr;
    const MethodInfo* kill = world ? a.class_get_method_from_name(world, "KillTile", 5) : nullptr;
    const MethodInfo* frame = world ? a.class_get_method_from_name(world, "TileFrameImportant", 5) : nullptr;
    const MethodInfo* largeDrop = world ? a.class_get_method_from_name(world, "KillTile_DropItems", 4) : nullptr;
    if (!safeTile(drop, 2, 9) || !safeTile(dust, 2, 3) ||
        !safeTile(frame, 3, 5) || !safeTile(largeDrop, 2, 4) || !kill) {
        BL_ERROR("[CSharp][Tile] Assinatura Terraria.Tile nao corresponde ao ABI nativo; hooks de quebra desativados");
        return false;
    }
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (main) {
        g_solidField = il2cpp::findField(main, "tileSolid");
        g_solidTopField = il2cpp::findField(main, "tileSolidTop");
    }
    g_killMethod = kill;
    g_largeDropMethod = largeDrop;
    if (!il2cpp::methodPointer(largeDrop) || !il2cpp::methodPointer(drop) ||
        !il2cpp::methodPointer(kill) || !il2cpp::methodPointer(dust) ||
        !il2cpp::methodPointer(frame)) {
        BL_ERROR("[CSharp][Tile] Assinatura ou ponte de hooks de quebra indisponivel");
        return false;
    }
    if (!g_dropInstalled) {
        if (!hook::install(drop, hkGetItemDrops, &g_origDrop)) {
            BL_ERROR("[CSharp][Tile] Nao foi possivel vincular os drops");
            return false;
        }
        g_dropInstalled = true;
    }
    if (!g_killInstalled) {
        if (!hook::install(kill, hkKillTile, &g_origKill)) {
            BL_ERROR("[CSharp][Tile] Hook KillTile indisponivel");
            return false;
        }
        g_killInstalled = true;
    }
    if (!g_makeDustInstalled) {
        if (!hook::install(dust, hkMakeDust, &g_origDust)) {
            BL_ERROR("[CSharp][Tile] Hook KillTile_MakeTileDust indisponivel");
            return false;
        }
        g_makeDustInstalled = true;
    }
    g_dustInstalled = g_killInstalled && g_makeDustInstalled;
    if (!g_frameInstalled) {
        if (!hook::install(frame, hkTileFrame, &g_origFrame)) {
            BL_ERROR("[CSharp][Tile] Hook TileFrameImportant indisponivel");
            return false;
        }
        g_frameInstalled = true;
    }
    Il2CppClass* dustClass = il2cpp::findClass({"Terraria", "Dust", {}});
    g_newDust = dustClass ? a.class_get_method_from_name(dustClass, "NewDust", 9) : nullptr;
    g_installed = g_dropInstalled && g_dustInstalled && g_frameInstalled;
    return g_installed;
}

}

void resetNativeTileSessionState() {
    tile_sprite::resetNativeTileSpritePass();
    g_tileScanTick=0;
    g_tileScanVisualCursor=0;
    g_tileScanBuffCursor=0;
    g_tileScanLightCursor=0;
    g_tileLightCache.clear();
    g_tileNearbyEvents.clear();
    g_lastUseFrames.clear();
    g_mouseOverFrames.clear();
    g_mouseOverFarFrames.clear();
}
void bindNativeTileAssembly(const std::string& mod,const std::string& assembly) {
    if(mod.empty()||assembly.empty())return;
    g_assemblyByMod[mod]=assembly;
    for(auto& [type,behavior]:g_tiles) {
        if(behavior.spec.mod!=mod)continue;
        behavior.spec.assembly=assembly;
        if((behavior.spec.callbackMask & tile_policy::kManagedDrawCallbacks)!=0 &&
           type>=runtime::kVanillaTileCount && type<65536 && g_managedDrawBound) {
            g_managedDrawBindings.publish(type,behavior);
            g_managedDrawUncached[type].store(!behavior.spec.cacheDrawData,std::memory_order_release);
            g_managedDrawMarks[type].store(1,std::memory_order_release);
        }
        if(behavior.spec.callbackMask)
            queueManagedTileWarmup(mod,assembly,behavior.spec.name);
    }
    if(g_managedDrawBound)runtime::invalidateManagedTileDrawCache();
}
bool registerNativeTileGameplay(const NativeTileGameplaySpec& spec) {
    if (spec.type < runtime::kVanillaTileCount || spec.type >= 65536 ||
        spec.shape < 0 || spec.shape > 4 ||
        spec.frameToggle < 0 || spec.frameToggle > 2 ||
        (spec.frameToggle==1&&spec.shape!=2) ||
        (spec.frameToggle==2&&spec.shape!=3) ||
        spec.failDust < 0 || spec.failDust > 20 ||
        spec.normalDust < 0 || spec.normalDust > 20 ||
        !tile_policy::validCallbackMask(spec.callbackMask) ||
         !tile_mining::valid(spec.minPick,spec.mineResist) ||
        !tile_animation::valid(spec.animationFrameHeight,
                               {spec.animationFrames, spec.animationFrameTicks})) return false;
    TileBehavior behavior{};
    behavior.spec = spec;
    const auto assembly=g_assemblyByMod.find(spec.mod);
    if(assembly!=g_assemblyByMod.end())behavior.spec.assembly=assembly->second;
    behavior.itemType = basicItemForTile(spec.mod + "/" + spec.name);
    behavior.dustType = resolveDustId(spec.dustName);
    if(spec.registeredDrops.size()>16)return false;
    bool needStyle=false;
    for(const auto& entry:spec.registeredDrops) {
        if(entry.style< -1||entry.style>4095||
           behavior.styleDrops.contains(entry.style))return false;
        int item=entry.itemType;
        if(item==0&&!entry.itemName.empty())
            item=runtime::modItemTypeByKey(spec.mod+"/"+entry.itemName);
        if(item<1||item>65535)return false;
        behavior.styleDrops.emplace(entry.style,item);
        needStyle|=entry.style>=0&&spec.shape!=0;
    }
    if(needStyle&&!bindTileStyleLookup()) {
        BL_ERROR("[CSharp][TileDrop] Mod=%s Class=%s Stage=BindTileStyle Result=Rejected",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if (!setupHooks()) return false;
    if((spec.minPick>0||spec.mineResist!=1.f)&&!setupNativeTileMiningHook()) {
        BL_ERROR("[CSharp][TileMining] Mod=%s Class=%s Stage=BindGetPickaxeDamage Result=Rejected",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if ((spec.animationFrames > 1 || (spec.callbackMask & ((1 << 6)|(1 << 11)|(1 << 12)))) &&
        !setupNativeTileAnimation()) return false;
    if((spec.callbackMask&((1<<6)|(1<<11))) &&
       !setupTileWorldScan((spec.callbackMask&(1<<6))!=0,(spec.callbackMask&(1<<11))!=0))
        return false;
    if ((!spec.allowBreaking || (spec.callbackMask&1)) && !setupCanKillTileHook()) {
        BL_ERROR("[CSharp][Tile] CanKillTile ABI ou hook indisponivel para %s/%s",
                 spec.mod.c_str(), spec.name.c_str());
        return false;
    }
    if(spec.campfire&&tile_toggle::allowsRightClick(spec.frameToggle)&&
       !setupNativeCampfireSound()) {
        BL_ERROR("[CSharp][CampfireSound] Mod=%s Class=%s Stage=BindSoundEngine Result=Rejected",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if(spec.campfire&&!setupNativeCampfireSceneHook()) {
        BL_ERROR("[CSharp][Campfire] Mod=%s Class=%s Stage=BindSceneMetrics Result=Rejected",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if(spec.frameToggle&&!setupNativeFrameToggle())return false;
    if(((spec.callbackMask&(1<<4))||tile_toggle::allowsRightClick(spec.frameToggle))&&
       !setupTileInteractHook())return false;
    if(tile_smart::eligible(spec.callbackMask,spec.frameToggle)&&
       !g_smartInteract.ready&&!g_smartInteractRejected&&
       !setupTileSmartInteractHook()) {
        g_smartInteractRejected=true;
        BL_WARN("[CSharp][TileSmart] Stage=Bind CandidateProviderABI=Unavailable Result=MobileCursorFallback");
    }
    if((spec.callbackMask&(1<<8))&&!setupTileSlopeHooks())return false;
    if((spec.callbackMask&(1<<21))&&!setupTileFrameCallbackHook())return false;
    if((spec.callbackMask&(1<<26))&&!setupNativeTileKillSoundHook())return false;
    if((spec.callbackMask&(1<<28))&&!setupNativeTileSaplingHook()) {
        BL_ERROR("[CSharp][GrowSapling] Mod=%s Class=%s Stage=BindNativeABI Result=Rejected",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if(!setupTileMouseOverHooks((spec.callbackMask&((1<<4)|(1<<9)))!=0||
                                 tile_toggle::allowsRightClick(spec.frameToggle),
                                (spec.callbackMask&(1<<10))!=0))return false;
    if((spec.callbackMask&(1<<7))&&!setupTilePlacementHooks())return false;
    if(!setupTileWorldHooks(((spec.callbackMask&4)!=0)||spec.frameToggle,(spec.callbackMask&8)!=0)) {
        BL_ERROR("[CSharp][Tile] World hook ABI incompatível Mod=%s Class=%s",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    if((spec.callbackMask & tile_policy::kManagedDrawCallbacks) != 0) {
        if((spec.callbackMask & ((1 << 5) | (15 << 16))) != 0) {
            std::string drawingError;
            if(!tile_sprite::installNativeTileSpritePass(&drawingError))
                BL_WARN("[CSharp][TileDraw] Stage=BindSpritePass Mod=%s Class=%s Result=DefaultFallback Detail=%s",
                        spec.mod.c_str(),spec.name.c_str(),drawingError.c_str());
        }
        if((spec.callbackMask&(1<<19))!=0) {
            std::string dustError;
            if(!tile_sprite::bindNativeTileDust(&dustError))
                BL_WARN("[CSharp][TileDust] Mod=%s Class=%s Stage=Bind Result=Disabled Detail=%s",
                        spec.mod.c_str(),spec.name.c_str(),dustError.c_str());
        }
        runtime::ensureTileDrawHook();
        if(!runtime::tileDrawHooksReady()) {
            BL_ERROR("[CSharp][TileDraw] Mod=%s Class=%s Stage=Bind Result=DrawHookUnavailable",
                spec.mod.c_str(),spec.name.c_str());
            return false;
        }
        if(!g_managedDrawBound) {
            runtime::setManagedTileDrawDataHook(applyManagedTileDraw,g_managedDrawMarks.data(),
                                                g_managedDrawUncached.data());
            g_managedDrawBound=true;
        }
    }
    g_tiles[spec.type] = behavior;
    if(spec.campfire)g_campfireTiles.insert(spec.type);
    if(spec.callbackMask&((1<<5)|(1<<16)))
        runtime::requestModTileOverlays(spec.type);
    if((spec.callbackMask & tile_policy::kManagedDrawCallbacks) != 0 && !behavior.spec.assembly.empty()) {
        g_managedDrawBindings.publish(spec.type,behavior);
        g_managedDrawUncached[spec.type].store(!spec.cacheDrawData,std::memory_order_release);
        g_managedDrawMarks[spec.type].store(1,std::memory_order_release);
    }
    g_usesManagedNearby|=(spec.callbackMask&(1<<11))!=0;
    g_usesManagedLight|=(spec.callbackMask&(1<<6))!=0;
    if(spec.callbackMask&& !behavior.spec.assembly.empty())
        queueManagedTileWarmup(spec.mod,behavior.spec.assembly,spec.name);
    if (spec.animationFrames > 1 || (spec.callbackMask & (1 << 12))) {
        g_animatedDrawFrames[spec.type].store(0,std::memory_order_relaxed);
        g_animatedTiles[spec.type] = {spec.animationFrames, spec.animationFrameTicks};
    }
    BL_DEBUG("[CSharp][Tile] %s/%s type=%d item=%d dust=%d shape=%d",
             spec.mod.c_str(), spec.name.c_str(), spec.type,
             behavior.itemType, behavior.dustType, spec.shape);
    return true;
}

}
