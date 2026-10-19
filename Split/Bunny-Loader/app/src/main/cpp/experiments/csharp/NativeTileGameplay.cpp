#include "experiments/csharp/NativeTileGameplay.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include "experiments/csharp/NativeTileAnimationPolicy.h"
#include "il2cpp/FieldContract.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/tiles/TileAccess.h"
#include "content/tiles/ModTiles.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>

namespace bl::csharp {
namespace {

struct TileBehavior {
    NativeTileGameplaySpec spec;
    int itemType = -1;
    int dustType = -1;
};
std::map<int, TileBehavior> g_tiles;
std::map<std::string,std::string> g_assemblyByMod;
bool g_wireInstalled = false;
bool g_overgroundInstalled = false;
bool g_undergroundInstalled = false;
bool g_interactInstalled=false;
bool g_slopeInstalled=false;
bool g_poundInstalled=false;
bool g_playerPlacementInstalled=false;
bool g_placeTileInstalled=false;
thread_local int g_playerPlaceDepth=0;
int g_useAttempt=-1,g_useRelease=-1,g_useHappened=-1;
FieldInfo* g_gameUpdateField=nullptr;
std::map<Il2CppObject*,uint64_t> g_lastUseFrames;
bool g_installed = false;
bool g_dropInstalled = false;
bool g_dustInstalled = false;
bool g_frameInstalled = false;
bool g_canKillInstalled = false;
bool g_animationInstalled = false;
using AnimateFn = void (*)(const MethodInfo*);
AnimateFn g_originalAnimate = nullptr;
FieldInfo* g_frameArray = nullptr;
FieldInfo* g_counterArray = nullptr;
std::map<int, tile_animation::Policy> g_animatedTiles;
thread_local int g_dustBudget = -1;
thread_local int g_dustOwner = -1;
thread_local bool g_frameCleanup = false;

using DropFn = void (*)(int32_t, int32_t, uint32_t, int32_t*, int32_t*,
                       int32_t*, int32_t*, bool*, bool, const MethodInfo*);
using KillFn = void (*)(int32_t, int32_t, bool, bool, bool, const MethodInfo*);
using DustFn = int32_t (*)(int32_t, int32_t, uint32_t, const MethodInfo*);
using FrameFn = void (*)(int32_t, int32_t, int32_t, uint32_t, bool, const MethodInfo*);
using CanKillFn = bool (*)(int32_t, int32_t, bool*, const MethodInfo*);
using LargeDropFn = void (*)(int32_t, int32_t, uint32_t, bool, const MethodInfo*);
DropFn g_origDrop = nullptr;
KillFn g_origKill = nullptr;
DustFn g_origDust = nullptr;
FrameFn g_origFrame = nullptr;
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
CanKillFn g_origCanKill = nullptr;
const MethodInfo* g_largeDropMethod = nullptr;
FieldInfo* g_solidField = nullptr;
FieldInfo* g_solidTopField = nullptr;
const MethodInfo* g_killMethod = nullptr;
const MethodInfo* g_newDust = nullptr;

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

bool managedTileReady(const NativeTileGameplaySpec& spec,int bit) {
    if((spec.callbackMask&(1<<bit))==0||spec.assembly.empty())return false;
    std::string error;
    return managedTileWarmupState(spec.mod,spec.assembly,spec.name,&error)==1;
}
bool invokeTileBool(const NativeTileGameplaySpec& spec,int bit,int x,int y,bool* out) {
    if(!out||!managedTileReady(spec,bit))return false;
    std::string reply;
    if(!invokeManagedTile(spec.mod,spec.assembly,spec.name,bit,x,y,&reply))return false;
    if(reply=="1"){*out=true;return true;}
    if(reply=="0"){*out=false;return true;}
    return false;
}
void dispatchTileEvent(const NativeTileGameplaySpec& spec,int bit,int x,int y) {
    if(managedTileReady(spec,bit))
        queueManagedTileEvent(spec.mod,spec.assembly,spec.name,bit,x,y);
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
    int itemType = behavior.itemType;
    if (itemType <= 0)
        itemType = basicItemForTile(behavior.spec.mod + "/" + behavior.spec.name);
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
    const int lastOwner = g_dustOwner, lastBudget = g_dustBudget;
    g_dustOwner = it->first;
    g_dustBudget = tile_policy::dustBudget(fail, it->second.spec.failDust,
                                           it->second.spec.normalDust);
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
    const int dust = spawnDust(x, y, it->second.dustType);
    return dust >= 0 ? dust : (g_origDust ? g_origDust(x, y, tile, method) : 6000);
}

void hkTileFrame(int32_t x, int32_t y, int32_t type, uint32_t tile,
                 bool reset, const MethodInfo* method) {
    const auto it = g_tiles.find(type);
    if (!g_frameInstalled || it == g_tiles.end() ||
        it->second.spec.shape != 1 || g_frameCleanup) {
        if (g_origFrame) g_origFrame(x, y, type, tile, reset, method);
        return;
    }
    runtime::TileArrays world{};
    if (!runtime::tileArrays(&world) || x < 0 || y < 0 ||
        x >= world.width || y >= world.height || !world.frameX) {
        if (g_origFrame) g_origFrame(x, y, type, tile, reset, method);
        return;
    }
    const int32_t index = y * world.width + x;
    const int left = tile_policy::leftOf2x1(x, world.frameX[world.lookup[index]]);
    const bool leftExists = cellIs(left, y, type, world);
    const bool rightExists = cellIs(left + 1, y, type, world);
    const bool supportedLeft = supports(left, y + 1, world);
    const bool supportedRight = supports(left + 1, y + 1, world);
    if (!tile_policy::incomplete2x1(leftExists, rightExists,
                                    supportedLeft, supportedRight)) return;
    g_frameCleanup = true;
    const int sourceX = leftExists ? left : left + 1;
    auto& a = il2cpp::api();
    if ((leftExists || rightExists) && g_largeDropMethod) {
        int32_t dropX = sourceX, dropY = y;
        int32_t sourceTile = y * world.width + sourceX;
        bool large = true;
        void* args[] = {&dropX, &dropY, &sourceTile, &large};
        Il2CppObject* exc = nullptr;
        a.runtime_invoke(g_largeDropMethod, nullptr, args, &exc);
        if (exc)
            BL_ERROR("[CSharp][Tile] Falha ao produzir drop de objeto 2x1");
    }
    if (g_killMethod) {
        for (int dx = 0; dx < 2; ++dx) {
            if (dx == 0 ? !leftExists : !rightExists) continue;
            int32_t killX = left + dx, killY = y;
            bool fail = false, effectOnly = false, noItem = true;
            void* args[] = {&killX, &killY, &fail, &effectOnly, &noItem};
            Il2CppObject* exc = nullptr;
            a.runtime_invoke(g_killMethod, nullptr, args, &exc);
            if (exc)
                BL_ERROR("[CSharp][Tile] Falha ao remover metade de objeto 2x1");
        }
    }
    g_frameCleanup = false;
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


void hkAnimateTiles(const MethodInfo* method) {
    if (g_originalAnimate) g_originalAnimate(method);
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
                        continue;
                    }
                }
            }
        }
        tile_animation::advance(frameValues[type], counterValues[type], policy);
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

void hkTileWire(int32_t x,int32_t y,const MethodInfo* method) {
    if(g_origWire)g_origWire(x,y,method);
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it!=g_tiles.end())dispatchTileEvent(it->second.spec,2,x,y);
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
    if(g_origUse)g_origUse(player,x,y,method);
    if(!player||!g_interactInstalled)return;
    const auto it=g_tiles.find(runtime::tileTypeAt(x,y));
    if(it==g_tiles.end()||(it->second.spec.callbackMask&(1<<4))==0)return;
    const auto* bytes=reinterpret_cast<const uint8_t*>(player);
    if(!bytes[g_useAttempt])return;
    uint64_t frame=0;
    il2cpp::api().field_static_get_value(g_gameUpdateField,&frame);
    auto old=g_lastUseFrames.find(player);
    if(old!=g_lastUseFrames.end()&&old->second==frame)return;
    const bool fresh=bytes[g_useRelease]||old==g_lastUseFrames.end()||
        (frame>old->second&&frame-old->second>1);
    g_lastUseFrames[player]=frame;
    if(!fresh)return;
    bool handled=false;
    if(invokeTileBool(it->second.spec,4,x,y,&handled)&&handled)
        reinterpret_cast<uint8_t*>(player)[g_useHappened]=1;
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
bool setupHooks() {
    if (g_installed) return g_dropInstalled && g_dustInstalled && g_frameInstalled;
    g_installed = true;
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
    if (!il2cpp::methodPointer(largeDrop) ||
        !hook::install(drop, hkGetItemDrops, &g_origDrop)) {
        BL_ERROR("[CSharp][Tile] Nao foi possivel vincular os drops");
        return false;
    }
    g_dropInstalled = true;
    g_dustInstalled = hook::install(kill, hkKillTile, &g_origKill) &&
                      hook::install(dust, hkMakeDust, &g_origDust);
    if (!g_dustInstalled)
        BL_ERROR("[CSharp][Tile] Dust/NumDust indisponivel; usando caminho vanilla");
    g_frameInstalled = g_dustInstalled &&
                       hook::install(frame, hkTileFrame, &g_origFrame);
    if (!g_frameInstalled)
        BL_ERROR("[CSharp][Tile] TileFrameImportant nao vinculado; limpeza de moveis indisponivel");
    Il2CppClass* dustClass = il2cpp::findClass({"Terraria", "Dust", {}});
    g_newDust = dustClass ? a.class_get_method_from_name(dustClass, "NewDust", 9) : nullptr;
    return g_dropInstalled && g_dustInstalled && g_frameInstalled;
}

}

void bindNativeTileAssembly(const std::string& mod,const std::string& assembly) {
    if(mod.empty()||assembly.empty())return;
    g_assemblyByMod[mod]=assembly;
    for(auto& [type,behavior]:g_tiles) {
        if(behavior.spec.mod!=mod)continue;
        behavior.spec.assembly=assembly;
        if(behavior.spec.callbackMask)
            queueManagedTileWarmup(mod,assembly,behavior.spec.name);
    }
}
bool registerNativeTileGameplay(const NativeTileGameplaySpec& spec) {
    if (spec.type < runtime::kVanillaTileCount || spec.shape < 0 || spec.shape > 1 ||
        spec.failDust < 0 || spec.failDust > 20 ||
        spec.normalDust < 0 || spec.normalDust > 20 ||
        !tile_animation::valid(spec.animationFrameHeight,
                               {spec.animationFrames, spec.animationFrameTicks})) return false;
    TileBehavior behavior{};
    behavior.spec = spec;
    const auto assembly=g_assemblyByMod.find(spec.mod);
    if(assembly!=g_assemblyByMod.end())behavior.spec.assembly=assembly->second;
    behavior.itemType = basicItemForTile(spec.mod + "/" + spec.name);
    behavior.dustType = resolveDustId(spec.dustName);
    if (!setupHooks()) return false;
    if ((spec.animationFrames > 1 || (spec.callbackMask & (1 << 12))) && !setupNativeTileAnimation()) return false;
    if ((!spec.allowBreaking || (spec.callbackMask&1)) && !setupCanKillTileHook()) {
        BL_ERROR("[CSharp][Tile] CanKillTile ABI ou hook indisponivel para %s/%s",
                 spec.mod.c_str(), spec.name.c_str());
        return false;
    }
    if((spec.callbackMask&(1<<4))&&!setupTileInteractHook())return false;
    if((spec.callbackMask&(1<<8))&&!setupTileSlopeHooks())return false;
    if((spec.callbackMask&(1<<7))&&!setupTilePlacementHooks())return false;
    if(!setupTileWorldHooks((spec.callbackMask&4)!=0,(spec.callbackMask&8)!=0)) {
        BL_ERROR("[CSharp][Tile] World hook ABI incompatível Mod=%s Class=%s",
                 spec.mod.c_str(),spec.name.c_str());
        return false;
    }
    g_tiles[spec.type] = behavior;
    if(spec.callbackMask&& !behavior.spec.assembly.empty())
        queueManagedTileWarmup(spec.mod,behavior.spec.assembly,spec.name);
    if (spec.animationFrames > 1 || (spec.callbackMask & (1 << 12)))
        g_animatedTiles[spec.type] = {spec.animationFrames, spec.animationFrameTicks};
    BL_DEBUG("[CSharp][Tile] %s/%s type=%d item=%d dust=%d shape=%d",
             spec.mod.c_str(), spec.name.c_str(), spec.type,
             behavior.itemType, behavior.dustType, spec.shape);
    return true;
}

}
