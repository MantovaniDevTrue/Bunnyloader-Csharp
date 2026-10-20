#include "experiments/csharp/NativeInvasionSpawnSite.h"

#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {

bool findNativeInvasionSpawnSite(int preferredX, int preferredY,
                                InvasionSpawnSite* result) {
    if (!result || preferredX < 0 || preferredY < 0) return false;
    *result = {};
    runtime::TileArrays world;
    if (!runtime::tileArrays(&world)) return false;

    using namespace il2cpp;
    auto* main = findClass({"Terraria", "Main", {}});
    if (!main) return false;
    auto* solidField = findField(main, "tileSolid");
    auto* topField = findField(main, "tileSolidTop");
    if (!solidField || !topField) return false;
    Il2CppArray* solid = nullptr;
    Il2CppArray* solidTop = nullptr;
    auto& a = il2cpp::api();
    a.field_static_get_value(solidField, &solid);
    a.field_static_get_value(topField, &solidTop);
    if (!solid || !solidTop) return false;
    InvasionTileView view;
    view.width = world.width;
    view.height = world.height;
    view.lookup = world.lookup;
    view.headers = world.sHeader;
    view.type = world.type;
    view.solid = static_cast<const uint8_t*>(arrayData(solid));
    view.solidTop = static_cast<const uint8_t*>(arrayData(solidTop));
    view.solidCount = solid->length;
    view.solidTopCount = solidTop->length;
    *result = findInvasionSpawnSite(view, preferredX, preferredY);
    return result->valid;
}

}
