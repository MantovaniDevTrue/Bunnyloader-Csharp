#pragma once
#include "content/items/ModItems.h"
#include <atomic>
#include <mutex>

namespace bl::runtime::item_registry {
struct Entry {
    ModItemDef def;
    int width = 0, height = 0;
    uint32_t asset = 0;   // gchandle do Asset<Texture2D>: reaplicado se a tabela for refeita
    uint32_t flameAsset = 0;   // a chama na mao (TextureAssets.ItemFlame)
    std::string unloadedKey;   // so da reserva "?": a chave do item ausente que ele representa
};

extern std::mutex g_mx;
extern std::vector<Entry> g_regs;
extern std::atomic<int> g_total, g_installed;
extern int g_poolFirst;
}
