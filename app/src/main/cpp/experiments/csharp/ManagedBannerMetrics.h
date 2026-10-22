#pragma once

#include "experiments/csharp/ManagedBannerLookupHooks.h"
#include "experiments/csharp/ManagedBannerSaveDispatch.h"
#include "experiments/csharp/ManagedBannerNearbyPolicy.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeTilePolicy.h"
#include "content/tiles/TileAccess.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <map>
#include <string>

namespace bl::csharp::banners {

using MetricsResetFn = void (*)(Il2CppObject*, const MethodInfo*);
inline MetricsResetFn originalBannerMetricsReset = nullptr;
inline bool bannerMetricsHookInstalled = false;
inline std::mutex bannerMetricsHookMutex;

inline bool bannerStrengthEnabled(int itemType) {
    if (itemType <= 0) return false;
    auto& api = il2cpp::api();
    if (!api.class_get_element_class || !api.class_from_il2cpp_type ||
        !api.class_is_valuetype || !api.class_value_size ||
        !api.object_get_class || !api.field_get_offset ||
        !api.field_get_flags || !api.field_static_get_value)
        return false;
    auto* item = il2cpp::findClass({"Terraria.ID", "ItemID", {}});
    auto* sets = item ? il2cpp::findNested(item, "Sets") : nullptr;
    auto* field = sets ? il2cpp::findField(sets, "BannerStrength") : nullptr;
    if (!field || (api.field_get_flags(field) & 0x10u) == 0) return false;
    Il2CppArray* values = nullptr;
    api.field_static_get_value(field, &values);
    if (!values || static_cast<uintptr_t>(itemType) >= values->length)
        return false;
    auto* cls = api.class_get_element_class(
        api.object_get_class(reinterpret_cast<Il2CppObject*>(values)));
    auto* enabled = cls ? il2cpp::findField(cls, "Enabled") : nullptr;
    if (!enabled || (api.field_get_flags(enabled) & 0x10u) != 0)
        return false;
    std::string error;
    FieldInfo* verified = nullptr;
    int position = -1;
    if (!il2cpp::nativeFieldContract(cls, "Enabled", "System.Boolean",
                                     false, 1, &verified, &position, &error))
        return false;

    if (api.class_is_valuetype(cls)) {
        uint32_t alignment = 0;
        const int stride = api.class_value_size(cls, &alignment);
        if (stride <= 0 || stride > 64 ||
            position < static_cast<int>(sizeof(Il2CppObject)) ||
            position - static_cast<int>(sizeof(Il2CppObject)) >= stride ||
            values->length > 262144)
            return false;
        const size_t offset = static_cast<size_t>(itemType) * stride +
            static_cast<size_t>(position - static_cast<int>(sizeof(Il2CppObject)));
        return static_cast<const uint8_t*>(arrayData(values))[offset] != 0;
    }
    if (values->length > 262144) return false;
    const auto* objects = static_cast<Il2CppObject* const*>(arrayData(values));
    const auto* entry = objects[itemType];
    if (!entry || api.object_get_class(const_cast<Il2CppObject*>(entry)) != cls)
        return false;
    return *(reinterpret_cast<const uint8_t*>(entry) + position) != 0;
}

inline void applyBannerSceneMetrics(Il2CppObject* metrics) {
    if (!metrics || !lookupsActive.load(std::memory_order_acquire))
        return;
    const uint64_t epoch = saveDispatch().worldEpoch();
    if (!epoch) return;
    auto& api = il2cpp::api();
    if (!api.field_get_flags || !api.field_static_get_value ||
        !api.object_get_class) return;
    auto* scene = il2cpp::findClass({"Terraria", "SceneMetrics", {}});
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!scene || !main || api.object_get_class(metrics) != scene) return;
    std::string error;
    FieldInfo* buff = nullptr;
    FieldInfo* has = nullptr;
    int buffOffset = -1, hasOffset = -1;
    if (!il2cpp::nativeFieldContract(scene,"NPCBannerBuff","System.Boolean[]",
                                     false,sizeof(void*),&buff,&buffOffset,&error) ||
        !il2cpp::nativeFieldContract(scene,"hasBanner","System.Boolean",
                                     false,1,&has,&hasOffset,&error))
        return;
    FieldInfo *posField=nullptr, *widthField=nullptr, *heightField=nullptr;
    FieldInfo *scanWidthField=nullptr, *scanHeightField=nullptr;
    if (!il2cpp::nativeFieldContract(main,"screenPosition",
                                     "Microsoft.Xna.Framework.Vector2",true,8,
                                     &posField,nullptr,&error) ||
        !il2cpp::nativeFieldContract(main,"screenWidth","System.Int32",true,4,
                                     &widthField,nullptr,&error) ||
        !il2cpp::nativeFieldContract(main,"screenHeight","System.Int32",true,4,
                                     &heightField,nullptr,&error) ||
        !il2cpp::nativeFieldContract(main,"buffScanAreaWidth","System.Int32",true,4,
                                     &scanWidthField,nullptr,&error) ||
        !il2cpp::nativeFieldContract(main,"buffScanAreaHeight","System.Int32",true,4,
                                     &scanHeightField,nullptr,&error))
        return;
    struct Vec {float x,y;} pos{};
    int width=0,height=0,scanWidth=0,scanHeight=0;
    api.field_static_get_value(posField,&pos);
    api.field_static_get_value(widthField,&width);
    api.field_static_get_value(heightField,&height);
    api.field_static_get_value(scanWidthField,&scanWidth);
    api.field_static_get_value(scanHeightField,&scanHeight);
    if (!std::isfinite(pos.x)||!std::isfinite(pos.y) ||
        pos.x<0 || pos.y<0 || pos.x>2000000 || pos.y>2000000 ||
        width<1 || width>8192 || height<1 || height>8192 ||
        scanWidth<1 || scanWidth>256 || scanHeight<1 || scanHeight>256)
        return;
    runtime::TileArrays world{};
    if (!runtime::tileArrays(&world) || !world.lookup || !world.frameX)
        return;
    int x=static_cast<int>(std::floor(pos.x/16.f));
    int y=static_cast<int>(std::floor(pos.y/16.f));
    x+=(width+15)/32-scanWidth/2;
    y+=(height+15)/32-scanHeight/2;
    const int left=std::clamp(x,0,world.width);
    const int top=std::clamp(y,0,world.height);
    const int right=std::clamp(x+scanWidth,0,world.width);
    const int bottom=std::clamp(y+scanHeight,0,world.height);
    const auto matches=nearbyBannerCache().near(
        epoch, nearbyBannerCache().latestTick(), left,top,right-left,bottom-top);
    if (matches.empty()) return;

    Il2CppArray* buffs=nullptr;
    std::memcpy(&buffs,reinterpret_cast<const char*>(metrics)+buffOffset,sizeof(buffs));
    if (!il2cpp::nativeArrayContract(buffs,"Boolean",1,262144)) return;
    auto* flags=static_cast<uint8_t*>(arrayData(buffs));
    const auto mappings=registry(runtime::kVanillaNpcCount).snapshot();
    std::map<int,bool> strengths;
    for (const auto& candidate : matches) {
        if (candidate.banner<runtime::kVanillaNpcCount ||
            static_cast<uintptr_t>(candidate.banner)>=buffs->length ||
            candidate.x>=world.width || candidate.y>=world.height ||
            runtime::tileTypeAt(candidate.x,candidate.y)!=candidate.tileType)
            continue;
        const size_t offset=static_cast<size_t>(candidate.y)*world.width+candidate.x;
        const int style=tile_policy::bannerStyleFromFrameX(
            world.frameX[world.lookup[offset]]);
        if (style!=candidate.style) continue;
        const int item=basicItemForTileStyle(
            runtime::modTileKey(candidate.tileType),style);
        const auto match=mappings->itemToBanner.find(item);
        if (item<=0 || match==mappings->itemToBanner.end() ||
            match->second!=candidate.banner) continue;
        auto strength=strengths.find(item);
        if (strength==strengths.end())
            strength=strengths.emplace(item,bannerStrengthEnabled(item)).first;
        if (!strength->second) continue;
        flags[candidate.banner]=1;
        *(reinterpret_cast<uint8_t*>(metrics)+hasOffset)=1;
    }
}

inline void hookBannerMetricsReset(Il2CppObject* metrics,const MethodInfo* method) {
    if (originalBannerMetricsReset)
        originalBannerMetricsReset(metrics,method);
    applyBannerSceneMetrics(metrics);
}

inline bool installBannerMetricsHook() {
    std::lock_guard<std::mutex> lock(bannerMetricsHookMutex);
    if (bannerMetricsHookInstalled) return true;
    auto* scene=il2cpp::findClass({"Terraria","SceneMetrics",{}});
    const MethodInfo* reset=scene?il2cpp::findMethodBySignature(
        scene,il2cpp::parseSignature("void Reset()")):nullptr;
    auto& api=il2cpp::api();
    if (!reset || !api.method_is_instance || !api.method_is_instance(reset))
        return false;
    if (!bl::hook::install(reset,hookBannerMetricsReset,&originalBannerMetricsReset))
        return false;
    bannerMetricsHookInstalled=true;
    return true;
}

} // namespace bl::csharp::banners
