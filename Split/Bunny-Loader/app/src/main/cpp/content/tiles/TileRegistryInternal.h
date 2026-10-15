#pragma once
#include "content/tiles/ModTiles.h"
#include <mutex>

namespace bl::runtime::tile_registry {
struct Entry {
    ModTileDef def;
    uint32_t asset = 0;   // gchandle do Asset<Texture2D>: reaplicado se a tabela for refeita
};

extern std::mutex g_mx;
extern std::vector<Entry> g_regs;
extern std::atomic<int> g_total, g_installed;
}
