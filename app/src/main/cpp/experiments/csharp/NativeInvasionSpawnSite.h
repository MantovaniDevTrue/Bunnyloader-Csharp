#pragma once

#include <algorithm>
#include <cstdint>

namespace bl::csharp {

struct InvasionTileView {
    int width = 0;
    int height = 0;
    const uint32_t* lookup = nullptr;
    const uint16_t* type = nullptr;
    const int16_t* headers = nullptr;
    const uint8_t* solid = nullptr;
    const uint8_t* solidTop = nullptr;
    uintptr_t solidCount = 0;
    uintptr_t solidTopCount = 0;
};

struct InvasionSpawnSite {
    int x = 0;
    int y = 0;
    bool valid = false;
};

namespace invasion_site {

constexpr int margin = 42;
constexpr int clearanceHalfWidth = 2;
constexpr int clearanceHeight = 6;
constexpr int groundHalfWidth = 1;
constexpr int horizontalOffsets[] = {0, -8, 8, -16, 16, -24, 24, -32, 32};
constexpr int16_t activeFlag = 0x20;
constexpr uint16_t invalidFloorShape = 0x7400;
constexpr int templeSpikes = 232;

inline bool occupied(const InvasionTileView& view, int x, int y, int* type) {
    if (!view.lookup || !view.headers || !view.type ||
        x < 0 || y < 0 || x >= view.width || y >= view.height)
        return false;
    const uint32_t definition = view.lookup[static_cast<int64_t>(y) * view.width + x];
    if ((view.headers[definition] & activeFlag) == 0) return false;
    *type = view.type[definition];
    return true;
}

inline bool ground(const InvasionTileView& view, int x, int y) {
    int type = -1;
    if (!occupied(view, x, y, &type) || type < 0 ||
        static_cast<uintptr_t>(type) >= view.solidCount ||
        static_cast<uintptr_t>(type) >= view.solidTopCount)
        return false;
    const uint32_t definition = view.lookup[static_cast<int64_t>(y) * view.width + x];
    const uint16_t header = static_cast<uint16_t>(view.headers[definition]);
    return view.solid[type] != 0 && view.solidTop[type] == 0 &&
        type != templeSpikes && (header & invalidFloorShape) == 0;
}

inline bool clear(const InvasionTileView& view, int x, int groundY) {
    for (int dy = 1; dy <= clearanceHeight; ++dy)
        for (int dx = -clearanceHalfWidth; dx <= clearanceHalfWidth; ++dx) {
            int occupiedType = -1;
            if (occupied(view, x + dx, groundY - dy, &occupiedType))
                return false;
        }
    for (int dx = -groundHalfWidth; dx <= groundHalfWidth; ++dx)
        if (!ground(view, x + dx, groundY)) return false;
    return true;
}

}

template <class Validator>
inline InvasionSpawnSite findInvasionSpawnSiteChecked(
    const InvasionTileView& view, int preferredX, int preferredY,
    Validator&& validator) {
    if (view.width < invasion_site::margin * 2 + 4 ||
        view.height < invasion_site::margin * 2 + 4 ||
        !view.lookup || !view.type || !view.headers ||
        !view.solid || !view.solidTop ||
        view.solidCount == 0 || view.solidTopCount == 0 ||
        preferredX < 0 || preferredY < 0)
        return {};
    const int centerX = preferredX / 16;
    const int centerY = preferredY / 16;
    for (const int offset : invasion_site::horizontalOffsets) {
        const int x = centerX + offset;
        if (x < invasion_site::margin || x >= view.width - invasion_site::margin) continue;
        const int yStart = std::max(invasion_site::margin, centerY - 12);
        const int yEnd = std::min(view.height - invasion_site::margin - 1, centerY + 40);
        for (int y = yStart; y <= yEnd; ++y)
            if (invasion_site::clear(view, x, y) &&
                validator(x * 16, y * 16))
                return {x * 16, y * 16, true};
    }
    return {};
}

inline InvasionSpawnSite findInvasionSpawnSite(
    const InvasionTileView& view, int preferredX, int preferredY) {
    return findInvasionSpawnSiteChecked(
        view, preferredX, preferredY, [](int, int) { return true; });
}

bool findNativeInvasionSpawnSite(int preferredX, int preferredY,
                                InvasionSpawnSite* result);

}
