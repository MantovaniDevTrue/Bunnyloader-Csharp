#include "experiments/csharp/NativeTileGameplay.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/tiles/TileAccess.h"
#include "content/tiles/ModTiles.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <algorithm>
#include <array>
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
bool g_installed = false;
bool g_dropInstalled = false;
bool g_dustInstalled = false;
bool g_frameInstalled = false;
thread_local int g_dustBudget = -1;
thread_local int g_dustOwner = -1;
thread_local bool g_frameCleanup = false;

using DropFn = void (*)(int32_t, int32_t, uint32_t, int32_t*, int32_t*,
                       int32_t*, int32_t*, bool*, bool, const MethodInfo*);
using KillFn = void (*)(int32_t, int32_t, bool, bool, bool, const MethodInfo*);
using DustFn = int32_t (*)(int32_t, int32_t, uint32_t, const MethodInfo*);
using FrameFn = void (*)(int32_t, int32_t, int32_t, uint32_t, bool, const MethodInfo*);
using LargeDropFn = void (*)(int32_t, int32_t, uint32_t, bool, const MethodInfo*);
DropFn g_origDrop = nullptr;
KillFn g_origKill = nullptr;
DustFn g_origDust = nullptr;
FrameFn g_origFrame = nullptr;
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

void hkGetItemDrops(int32_t x, int32_t y, uint32_t tile,
                    int32_t* drop, int32_t* stack, int32_t* extra, int32_t* extraStack,
                    bool* noPrefix, bool large, const MethodInfo* method) {
    if (g_origDrop) g_origDrop(x, y, tile, drop, stack, extra, extraStack, noPrefix, large, method);
    if (!g_dropInstalled || !drop || !stack || !extra || !extraStack) return;
    const auto it = g_tiles.find(runtime::tileTypeAt(x, y));
    if (it == g_tiles.end()) return;
    const auto& behavior = it->second;
    if (!tile_policy::shouldDrop(behavior.spec.shape, large)) {
        *drop = 0; *stack = 0; *extra = 0; *extraStack = 0;
        return;
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

bool registerNativeTileGameplay(const NativeTileGameplaySpec& spec) {
    if (spec.type < runtime::kVanillaTileCount || spec.shape < 0 || spec.shape > 1 ||
        spec.failDust < 0 || spec.failDust > 20 ||
        spec.normalDust < 0 || spec.normalDust > 20) return false;
    TileBehavior behavior{};
    behavior.spec = spec;
    behavior.itemType = basicItemForTile(spec.mod + "/" + spec.name);
    behavior.dustType = resolveDustId(spec.dustName);
    g_tiles[spec.type] = behavior;
    if (!setupHooks()) return false;
    BL_DEBUG("[CSharp][Tile] %s/%s type=%d item=%d dust=%d shape=%d",
             spec.mod.c_str(), spec.name.c_str(), spec.type,
             behavior.itemType, behavior.dustType, spec.shape);
    return true;
}

}
