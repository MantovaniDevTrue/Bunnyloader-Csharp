#include "content/tiles/ModTrees.h"
#include "content/tiles/ModTiles.h"
#include "content/tiles/TileAccess.h"
#include "experiments/csharp/NativeTreeDropPolicy.h"
#include "experiments/csharp/NativeTreePlantPolicy.h"
#include "content/common/GameRefs.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <atomic>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace bl::runtime {

namespace {

constexpr int kTreeType = 5;            // TileID.Trees
constexpr size_t kMaxGroundTypes = 65536;
constexpr int kMaxTrees = 255;
constexpr int kMaxTrunk = 300;          // altura máxima procurada até o chão

struct Tree {
    uint32_t trunk = 0;   // gchandle do Texture2D do tronco
    int topStyle = -1;    // índice em TextureAssets.TreeTop/TreeBranch
    std::atomic<int> woodItem{-1};
    std::atomic<bool> allowAcorn{true};
    std::atomic<int> saplingType{-1};
};

// O chão -> 1 + índice em g_trees (0 = árvore do jogo).
std::atomic<uint8_t> g_treeOf[kMaxGroundTypes];
Tree g_trees[kMaxTrees];
std::atomic<int> g_treeCount{0};   // só a thread do jogo escreve; a árvore vale depois do g_treeOf

const Tree* treeOfGround(int ground) {
    if (ground < 0 || static_cast<size_t>(ground) >= kMaxGroundTypes) return nullptr;
    const uint8_t k = g_treeOf[ground].load(std::memory_order_acquire);
    return k ? &g_trees[k - 1] : nullptr;
}

int typeAt(const TileArrays& t, int x, int y) {
    if (x < 0 || y < 0 || x >= t.width || y >= t.height) return -1;
    const uint32_t def = t.lookup[static_cast<int64_t>(t.width) * y + x];
    return (t.sHeader[def] & kTileActiveBit) ? t.type[def] : -1;
}

// O chão debaixo do tronco que passa por (x, y), descendo pelo tronco.
int groundBelow(const TileArrays& t, int x, int y, int* floorY) {
    for (int n = 0; typeAt(t, x, y) == kTreeType && n < kMaxTrunk; ++n) ++y;
    if (floorY) *floorY = y;
    return typeAt(t, x, y);
}

// Galho e raiz ficam ao lado do tronco: a coluna dele (TileDrawing.GetTreeBiome).
int trunkColumn(int x, int frameX, int frameY) {
    if (frameX == 66 && frameY <= 45) return x + 1;
    if (frameX == 88 && frameY >= 66 && frameY <= 110) return x - 1;
    if (frameY >= 198) {
        if (frameX == 44) return x + 1;
        if (frameX == 66) return x - 1;
    } else if (frameY >= 132) {
        if (frameX == 22) return x - 1;
        if (frameX == 44) return x + 1;
    }
    return x;
}

// ---- tronco: GetTileDrawData marca a célula, SpriteBatch.Draw troca a textura ----
//
// O DrawTiles lê a textura do tipo (TextureAssets.Tile[5]) e desenha a célula
// com uma das quatro sobrecargas de SpriteBatch.Draw/Draw_Fast logo depois do
// GetTileDrawData dela, na mesma thread. A marca vale até o próximo
// GetTileDrawData desta thread; só a textura do tronco do jogo é trocada.

thread_local Il2CppObject* t_trunk = nullptr;
FieldInfo* g_tileAssets = nullptr;          // TextureAssets.Tile
std::atomic<int32_t> g_assetValue{-1};      // Asset<Texture2D>.Value (da classe instanciada)

// O tronco do jogo agora (um pacote de texturas pode trocar o Asset).
Il2CppObject* vanillaTrunk() {
    Il2CppArray* assets = nullptr;
    il2cpp::api().field_static_get_value(g_tileAssets, &assets);
    if (!assets || assets->length <= static_cast<uintptr_t>(kTreeType)) return nullptr;
    Il2CppObject* asset = static_cast<Il2CppObject**>(arrayData(assets))[kTreeType];
    if (!asset) return nullptr;
    int32_t offset = g_assetValue.load(std::memory_order_relaxed);
    if (offset < 0) {
        offset = il2cpp::fieldOffset(il2cpp::api().object_get_class(asset), "Value");
        if (offset < 0) return nullptr;
        g_assetValue.store(offset, std::memory_order_relaxed);
    }
    return field<Il2CppObject*>(asset, offset);
}

// O tipo do parâmetro `k` de `mi` termina com `suffix` (exato com `exact`).
bool paramIs(const MethodInfo* mi, int k, const char* text, bool exact) {
    auto& a = il2cpp::api();
    char* name = a.type_get_name(a.method_get_param(mi, k));
    bool ok = false;
    if (name) {
        const size_t n = std::strlen(name), t = std::strlen(text);
        ok = exact ? std::strcmp(name, text) == 0
                   : (n >= t && std::strcmp(name + n - t, text) == 0) || std::strncmp(name, text, t) == 0;
    }
    std::free(name);
    return ok;
}

// Os registradores de argumento passam como vieram (os floats nos d0-d7, sem
// conversão): serve às quatro sobrecargas, que só têm a textura em x1.
using DrawFn = void (*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t,
                        double, double, double, double, double, double, double, double);
DrawFn g_origDraw[4] = {};

template <int K>
void hkDraw(intptr_t x0, intptr_t x1, intptr_t x2, intptr_t x3, intptr_t x4, intptr_t x5, intptr_t x6, intptr_t x7,
            double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7) {
    if (Il2CppObject* trunk = t_trunk) {
        if (reinterpret_cast<Il2CppObject*>(x1) == vanillaTrunk()) x1 = reinterpret_cast<intptr_t>(trunk);
    }
    g_origDraw[K](x0, x1, x2, x3, x4, x5, x6, x7, d0, d1, d2, d3, d4, d5, d6, d7);
}

// ---- o tronco em pé: WorldGen.CheckTree ----
//
// O CheckTree só aceita debaixo do tronco os chãos do jogo (compara com uma
// lista fixa, que vira "2") e quebra o resto. Durante ele, o chão de mod finge
// ser grama; o TileData.SetType reinterna o tile (escrever o tipo direto na
// definição mudaria todos os tiles iguais do mundo).

constexpr int kGrass = 2;
const MethodInfo* g_setType = nullptr;   // TileData.SetType(int tileIndex, ushort newType)

void setTypeAt(const TileArrays& t, int x, int y, int type) {
    using Fn = void (*)(int32_t, uint16_t, const MethodInfo*);
    reinterpret_cast<Fn>(il2cpp::methodPointer(g_setType))(t.width * y + x, static_cast<uint16_t>(type), g_setType);
}

using CheckTreeFn = void (*)(int32_t, int32_t, const MethodInfo*);
CheckTreeFn g_origCheckTree = nullptr;

void hkCheckTree(int32_t i, int32_t j, const MethodInfo* m) {
    TileArrays t;
    const int below = tileArrays(&t) ? typeAt(t, i, j + 1) : -1;
    if (below < 0 || !treeOfGround(below)) return g_origCheckTree(i, j, m);
    setTypeAt(t, i, j + 1, kGrass);
    g_origCheckTree(i, j, m);
    TileArrays after;
    if (tileArrays(&after) && typeAt(after, i, j + 1) == kGrass) setTypeAt(after, i, j + 1, below);
}

// ---- copa e galhos: o estilo da árvore de mod ----

using FoliageFn = bool (*)(int32_t, int32_t, int32_t, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*,
                           const MethodInfo*);
FoliageFn g_origFoliage = nullptr;

bool hkGetCommonTreeFoliageData(int32_t i, int32_t j, int32_t xoffset, int32_t* treeFrame, int32_t* treeStyle,
                                int32_t* floorY, int32_t* topWidth, int32_t* topHeight, const MethodInfo* m) {
    TileArrays t;
    int y = j;
    const Tree* tree = tileArrays(&t) ? treeOfGround(groundBelow(t, i + xoffset, j, &y)) : nullptr;
    if (!tree) return g_origFoliage(i, j, xoffset, treeFrame, treeStyle, floorY, topWidth, topHeight, m);
    *treeStyle = tree->topStyle;
    *floorY = y;
    *topWidth = 80;
    *topHeight = 80;
    return true;
}



using PlaceTreeFn=bool (*)(int32_t,int32_t,int32_t,int32_t,int32_t,void*,bool,uint64_t,const MethodInfo*);
PlaceTreeFn g_originalPlaceTree=nullptr;
bool g_treePlantHooked=false;

bool hkPlaceModTreeSapling(int32_t x,int32_t y,int32_t type,int32_t style,int32_t dir,
                          void* objectData,bool onlyCheck,uint64_t forcedRandom,const MethodInfo* method) {
    if(!g_originalPlaceTree)return false;
    if(type==bl::csharp::tree_plant::vanillaSapling) {
        TileArrays t;
        if(tileArrays(&t)&&x>=0&&y>=0&&x<t.width&&y<t.height-1) {
            const int ground=typeAt(t,x,y+1);
            const Tree* tree=treeOfGround(ground);
            if(tree) {
                int nextType=type,nextStyle=style;
                if(bl::csharp::tree_plant::rewrite(type,ground,ground,
                       tree->saplingType.load(std::memory_order_acquire),
                       &nextType,&nextStyle)) {
                    type=nextType;
                    style=nextStyle;
                }
            }
        }
    }
    return g_originalPlaceTree(x,y,type,style,dir,objectData,onlyCheck,forcedRandom,method);
}

bool setupNativeTreePlantingHook() {
    if(g_treePlantHooked)return true;
    auto& a=il2cpp::api();
    if(sizeof(void*)!=8||!a.method_is_instance||!a.method_get_param_count||
       !a.method_get_param||!a.method_get_return_type||!a.class_from_il2cpp_type||
       !a.class_is_valuetype||!a.class_value_size||!a.type_get_name||
       !a.il2cpp_free)return false;
    auto* object=il2cpp::findClass({"Terraria","TileObject",{}});
    const auto* method=object?a.class_get_method_from_name(object,"CanPlace",8):nullptr;
    if(!method||a.method_is_instance(method)||a.method_get_param_count(method)!=8||
       !paramIs(method,0,"System.Int32",true)||
       !paramIs(method,1,"System.Int32",true)||
       !paramIs(method,2,"System.Int32",true)||
       !paramIs(method,3,"System.Int32",true)||
       !paramIs(method,4,"System.Int32",true)||
       !paramIs(method,5,"Terraria.TileObject&",true)||
       !paramIs(method,6,"System.Boolean",true)||
       !paramIs(method,7,"System.Nullable",false)||
       !il2cpp::methodPointer(method))return false;
    const auto* nullableType=a.method_get_param(method,7);
    char* nullableName=a.type_get_name(nullableType);
    const bool isNullableInt=nullableName&&
        std::strstr(nullableName,"System.Int32")!=nullptr;
    if(nullableName)std::free(nullableName);
    auto* nullableClass=a.class_from_il2cpp_type(nullableType);
    uint32_t alignment=0;
    if(!isNullableInt||!nullableClass||!a.class_is_valuetype(nullableClass)||
       a.class_value_size(nullableClass,&alignment)!=8)return false;
    char* result=a.type_get_name(a.method_get_return_type(method));
    const bool returnsBool=result&&std::strcmp(result,"System.Boolean")==0;
    if(result)std::free(result);
    if(!returnsBool||!hook::install(method,hkPlaceModTreeSapling,&g_originalPlaceTree))
        return false;
    g_treePlantHooked=true;
    return true;
}

using TreeDropsFn=void (*)(int32_t,int32_t,uint32_t,bool*,bool*,int32_t*,int32_t*,const MethodInfo*);
TreeDropsFn g_originalTreeDrops=nullptr;
bool g_treeDropsHooked=false;
int g_acornType=-1;

bool readNativeAcornType() {
    if(g_acornType>0)return true;
    auto& a=il2cpp::api();
    auto* cls=il2cpp::findClass({"Terraria.ID","ItemID",{}});
    auto* f=cls?il2cpp::findField(cls,"Acorn"):nullptr;
    if(!f||!a.field_get_type||!a.field_get_flags||!a.type_get_name)return false;
    if((a.field_get_flags(f)&0x40u)==0)return false;
    char* typeName=a.type_get_name(a.field_get_type(f));
    const bool rightType=typeName&&std::strcmp(typeName,"System.Int32")==0;
    if(typeName)std::free(typeName);
    if(!rightType)return false;
    using DefaultFn=const void* (*)(const FieldInfo*,const Il2CppType**);
    auto* getDefault=reinterpret_cast<DefaultFn>(dlsym(RTLD_DEFAULT,"il2cpp_field_get_default_value"));
    if(!getDefault)return false;
    const Il2CppType* outType=nullptr;
    const void* value=getDefault(f,&outType);
    int acorn=-1;
    if(value)std::memcpy(&acorn,value,sizeof(acorn));
    if(acorn<=0||acorn>=6196)return false;
    g_acornType=acorn;
    return true;
}

void hkGetTreeDrops(int32_t x,int32_t y,uint32_t tile,
                    bool* isWood,bool* bonusWood,int32_t* primary,int32_t* secondary,
                    const MethodInfo* method) {
    if(g_originalTreeDrops)
        g_originalTreeDrops(x,y,tile,isWood,bonusWood,primary,secondary,method);
    if(!primary||!secondary||!isWood)return;
    TileArrays t;
    if(!tileArrays(&t)||x<0||x>=t.width||y<0||y>=t.height||
       typeAt(t,x,y)!=kTreeType)return;
    const uint32_t def=t.lookup[static_cast<int64_t>(t.width)*y+x];
    const int ground=groundBelow(t,trunkColumn(x,t.frameX[def],t.frameY[def]),y,nullptr);
    const Tree* tree=treeOfGround(ground);
    if(!tree)return;
    bl::csharp::tree_drop::apply(*isWood,g_acornType,
        tree->woodItem.load(std::memory_order_acquire),
        tree->allowAcorn.load(std::memory_order_acquire),primary,secondary);
}

bool installHooks() {
    using namespace il2cpp;
    auto& a = api();
    Il2CppClass* textures = findClass({"Terraria.GameContent", "TextureAssets", {}});
    g_tileAssets = textures ? findField(textures, "Tile") : nullptr;
    if (!g_tileAssets) {
        BL_ERROR("árvores de mod: TextureAssets.Tile não achado; o tronco sai com a textura do jogo");
    } else {
        // As que o TileDrawing.DrawTiles usa (a textura sempre em x1):
        //   Draw(Texture2D, Vector2, Nullable<Rectangle>, Color, float, Vector2, float, SpriteEffects, float)
        //   Draw(Texture2D, Vector2, ref Rectangle, Color, float, Vector2, float, SpriteEffects, float)
        //   Draw_Fast(Texture2D, ref Vector2, ref Rectangle, ref VertexColors | ref Color, SpriteEffects)
        Il2CppClass* batch = findClass({"Microsoft.Xna.Framework.Graphics", "SpriteBatch", {}});
        const struct { const char* name; uint32_t params; bool (*match)(const MethodInfo*); } draws[] = {
            {"Draw", 9, [](const MethodInfo* mi) { return paramIs(mi, 2, "System.Nullable", false) && paramIs(mi, 3, "Graphics.Color", false) && paramIs(mi, 6, "System.Single", true); }},
            {"Draw", 9, [](const MethodInfo* mi) { return paramIs(mi, 2, "Rectangle&", false) && paramIs(mi, 3, "Graphics.Color", false) && paramIs(mi, 6, "System.Single", true); }},
            {"Draw_Fast", 5, [](const MethodInfo* mi) { return paramIs(mi, 3, "VertexColors&", false); }},
            {"Draw_Fast", 5, [](const MethodInfo* mi) { return paramIs(mi, 3, "Graphics.Color&", false); }},
        };
        DrawFn hooks[4] = {hkDraw<0>, hkDraw<1>, hkDraw<2>, hkDraw<3>};
        for (int k = 0; k < 4; ++k) {
            const MethodInfo* found = nullptr;
            void* it = nullptr;
            while (const MethodInfo* mi = batch ? a.class_get_methods(batch, &it) : nullptr) {
                if (std::strcmp(a.method_get_name(mi), draws[k].name) == 0 &&
                    a.method_get_param_count(mi) == draws[k].params && draws[k].match(mi)) found = mi;
            }
            if (!found || !hook::install(found, hooks[k], &g_origDraw[k])) {
                BL_ERROR("árvores de mod: sem hook no SpriteBatch.%s (%d); parte do tronco sai com a textura do jogo",
                         draws[k].name, k);
            }
        }
    }
    ensureTileDrawHook();

    Il2CppClass* worldGen = findClass({"Terraria", "WorldGen", {}});
    Il2CppClass* data = findClass({"Terraria", "TileData", {}});
    g_setType = data ? a.class_get_method_from_name(data, "SetType", 2) : nullptr;
    const MethodInfo* check = worldGen ? a.class_get_method_from_name(worldGen, "CheckTree", 2) : nullptr;
    if (!g_setType || !check || !hook::install(check, hkCheckTree, &g_origCheckTree)) {
        BL_ERROR("árvores de mod: sem hook no WorldGen.CheckTree; a árvore de mod se quebra ao crescer");
    }
    const MethodInfo* foliage = worldGen ? a.class_get_method_from_name(worldGen, "GetCommonTreeFoliageData", 8) : nullptr;
    if (!foliage || !hook::install(foliage, hkGetCommonTreeFoliageData, &g_origFoliage)) {
        BL_ERROR("árvores de mod: sem hook no WorldGen.GetCommonTreeFoliageData; copa e galhos saem como os da floresta");
    }
    return true;
}

} // namespace

void noteTreeDrawTile(int x, int y, int type, int frameX, int frameY) {
    if (type != kTreeType || g_treeCount.load(std::memory_order_relaxed) == 0) {
        t_trunk = nullptr;
        return;
    }
    TileArrays t;
    const Tree* tree = tileArrays(&t) ? treeOfGround(groundBelow(t, trunkColumn(x, frameX, frameY), y, nullptr)) : nullptr;
    t_trunk = tree ? il2cpp::api().gchandle_get_target(tree->trunk) : nullptr;
}

bool prepareModTreeDrops(bool needAcorn) {
    if(needAcorn&&!readNativeAcornType())return false;
    if(g_treeDropsHooked)return true;
    auto& a=il2cpp::api();
    if(!a.method_is_instance||!a.method_get_param_count||
       !a.method_get_return_type||!a.class_from_il2cpp_type||
       !a.class_value_size||!a.class_is_valuetype)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    const auto* method=world?a.class_get_method_from_name(world,"KillTile_GetTreeDrops",7):nullptr;
    if(!method||a.method_is_instance(method)||a.method_get_param_count(method)!=7||
       !paramIs(method,0,"System.Int32",true)||
       !paramIs(method,1,"System.Int32",true)||
       !paramIs(method,2,"Terraria.Tile",true)||
       !paramIs(method,3,"System.Boolean&",true)||
       !paramIs(method,4,"System.Boolean&",true)||
       !paramIs(method,5,"System.Int32&",true)||
       !paramIs(method,6,"System.Int32&",true)||
       !il2cpp::methodPointer(method))return false;
    char* typeName=a.type_get_name(a.method_get_return_type(method));
    const bool returnsVoid=typeName&&std::strcmp(typeName,"System.Void")==0;
    if(typeName)std::free(typeName);
    auto* tileType=a.class_from_il2cpp_type(a.method_get_param(method,2));
    uint32_t alignment=0;
    if(!returnsVoid||!tileType||!a.class_is_valuetype(tileType)||
       a.class_value_size(tileType,&alignment)!=4)return false;
    if(!hook::install(method,hkGetTreeDrops,&g_originalTreeDrops))return false;
    g_treeDropsHooked=true;
    return true;
}

bool configureModTreeDrops(int groundType,int woodItem,bool allowAcorn) {
    Tree* tree=const_cast<Tree*>(treeOfGround(groundType));
    if(!tree||!bl::csharp::tree_drop::valid(woodItem,allowAcorn))return false;
    if((woodItem>=0||!allowAcorn)&&!prepareModTreeDrops(!allowAcorn))return false;
    tree->woodItem.store(woodItem,std::memory_order_release);
    tree->allowAcorn.store(allowAcorn,std::memory_order_release);
    return true;
}

bool prepareModTreePlanting() {
    return setupNativeTreePlantingHook();
}

bool configureModTreeSapling(int groundType,int saplingType) {
    Tree* tree=const_cast<Tree*>(treeOfGround(groundType));
    if(!tree||!bl::csharp::tree_plant::valid(groundType,saplingType)||
       !g_treePlantHooked)return false;
    tree->saplingType.store(saplingType,std::memory_order_release);
    return true;
}

bool hasModTree(int groundType) { return treeOfGround(groundType)!=nullptr; }
int modTreeCount() {return g_treeCount.load(std::memory_order_acquire);}

bool registerModTreeChecked(int groundType, Il2CppObject* trunk, int topStyle) {
    if (groundType < 0 || static_cast<size_t>(groundType) >= kMaxGroundTypes || !trunk ||
        topStyle<0 || hasModTree(groundType)) return false;
    const int count = g_treeCount.load(std::memory_order_relaxed);
    if (count >= kMaxTrees) {
        BL_ERROR("árvores de mod: mais de %d árvores; a de chão %d ficou de fora", kMaxTrees, groundType);
        return false;
    }
    static std::once_flag once;
    std::call_once(once, installHooks);

    Tree& tree = g_trees[count];
    tree.trunk = il2cpp::api().gchandle_new(trunk, false);
    if (!tree.trunk) return false;
    tree.topStyle = topStyle;
    g_treeCount.store(count + 1, std::memory_order_release);
    g_treeOf[groundType].store(static_cast<uint8_t>(count + 1), std::memory_order_release);
    return true;
}

void setModTree(int groundType, Il2CppObject* trunk, int topStyle) {
    (void)registerModTreeChecked(groundType,trunk,topStyle);
}

} // namespace bl::runtime
