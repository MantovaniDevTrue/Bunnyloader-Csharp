#pragma once

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

InvasionSpawnSite findInvasionSpawnSite(
    const InvasionTileView& tiles, int preferredX, int preferredY);

bool findNativeInvasionSpawnSite(int preferredX, int preferredY,
                                InvasionSpawnSite* result);

}
