#pragma once
#include "content/tiles/ModTiles.h"
#include <mutex>
#include <array>

namespace bl::runtime::tile_registry {
struct Entry {
    ModTileDef def;
    uint32_t asset = 0;   // gchandle do Asset<Texture2D>: reaplicado se a tabela for refeita
    bool overlaysRequested = false;
    bool overlaysAttempted = false;
    std::array<uint32_t,2> overlays{};
    std::array<int,2> overlayWidth{};
    std::array<int,2> overlayHeight{};
};

extern std::mutex g_mx;
extern std::vector<Entry> g_regs;
extern std::atomic<int> g_total, g_installed;
}
