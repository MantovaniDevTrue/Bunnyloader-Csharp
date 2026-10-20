#include "experiments/csharp/NativeInvasionSpawnSite.h"

#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

constexpr int kMargin = 42;
constexpr int kClearanceHalfWidth = 2;
constexpr int kClearanceHeight = 6;
constexpr int kGroundHalfWidth = 1;
constexpr int kOffsets[] = {0, -8, 8, -16, 16, -24, 24, -32, 32};

bool lookupTile(const InvasionTileView& view, int x, int y, int* type) {
    if (!view.lookup || !view.headers || !view.type ||
        x < 0 || y < 0 || x >= view.width || y >= view.height)
        return false;
    const uint32_t definition = view.lookup[static_cast<int64_t>(y) * view.width + x];
    if ((view.headers[definition] & runtime::kTileActiveBit) == 0) return false;
    *type = view.type[definition];
    return true;
}

bool ground(const InvasionTileView& view, int x, int y) {
    int type = -1;
    if (!lookupTile(view, x, y, &type) ||
        type < 0 || static_cast<uintptr_t>(type) >= view.solidCount ||
        static_cast<uintptr_t>(type) >= view.solidTopCount)
        return false;
    return view.solid[type] != 0 && view.solidTop[type] == 0;
}

bool freeSpace(const InvasionTileView& view, int x, int groundY) {
    for (int dy = 1; dy <= kClearanceHeight; ++dy)
        for (int dx = -kClearanceHalfWidth; dx <= kClearanceHalfWidth; ++dx) {
            int occupiedType = -1;
            if (lookupTile(view, x + dx, groundY - dy, &occupiedType))
                return false;
        }
    for (int dx = -kGroundHalfWidth; dx <= kGroundHalfWidth; ++dx)
        if (!ground(view, x + dx, groundY)) return false;
    return true;
}

}

InvasionSpawnSite findInvasionSpawnSite(
    const InvasionTileView& view, int preferredX, int preferredY) {
    if (view.width < kMargin * 2 + 4 || view.height < kMargin * 2 + 4 ||
        !view.lookup || !view.type || !view.headers ||
        !view.solid || !view.solidTop ||
        view.solidCount == 0 || view.solidTopCount == 0 ||
        preferredX < 0 || preferredY < 0)
        return {};
    const int centerX = preferredX / 16;
    const int centerY = preferredY / 16;
    for (const int offset : kOffsets) {
        const int x = centerX + offset;
        if (x < kMargin || x >= view.width - kMargin) continue;
        const int yStart = std::max(kMargin, centerY - 12);
        const int yEnd = std::min(view.height - kMargin - 1, centerY + 40);
        for (int y = yStart; y <= yEnd; ++y)
            if (freeSpace(view, x, y))
                return {x * 16, y * 16, true};
    }
    return {};
}

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
