#include "content/tiles/TileRegistryInternal.h"

namespace bl::runtime::tile_registry {
std::mutex g_mx;
std::vector<Entry> g_regs;   // indice = type - kVanillaTileCount
std::atomic<int> g_total{0};
std::atomic<int> g_installed{0};
}
namespace bl::runtime {
using namespace tile_registry;
int registerModTile(ModTileDef def) {
    std::lock_guard<std::mutex> l(g_mx);
    for (const Entry& e : g_regs) {
        if (e.def.mod == def.mod && e.def.name == def.name) return -1;
    }
    Entry e;
    e.def = std::move(def);
    g_regs.push_back(std::move(e));
    g_total.store(static_cast<int>(g_regs.size()), std::memory_order_release);
    return kVanillaTileCount + static_cast<int>(g_regs.size()) - 1;
}

bool isModTile(int type) {
    return type >= kVanillaTileCount && type < kVanillaTileCount + g_total.load(std::memory_order_acquire);
}

int tileTypeCount() {
    return kVanillaTileCount + g_installed.load(std::memory_order_acquire);
}

int modTileTypeByName(const std::string& mod, const std::string& name) {
    std::lock_guard<std::mutex> l(g_mx);
    for (size_t i = 0; i < g_regs.size(); ++i) {
        if (g_regs[i].def.mod == mod && g_regs[i].def.name == name) {
            return kVanillaTileCount + static_cast<int>(i);
        }
    }
    return -1;
}

std::string modTileKey(int type) {
    std::lock_guard<std::mutex> l(g_mx);
    const size_t i = static_cast<size_t>(type - kVanillaTileCount);
    if (type < kVanillaTileCount || i >= g_regs.size()) return {};
    return g_regs[i].def.mod + "/" + g_regs[i].def.name;
}

int modTileTypeByKey(const std::string& key) {
    const size_t slash = key.find('/');
    if (slash == std::string::npos) return -1;
    return modTileTypeByName(key.substr(0, slash), key.substr(slash + 1));
}

}
