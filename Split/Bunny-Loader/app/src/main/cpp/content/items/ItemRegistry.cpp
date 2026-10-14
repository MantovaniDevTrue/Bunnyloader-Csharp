#include "content/items/ItemRegistryInternal.h"

namespace bl::runtime::item_registry {
std::mutex g_mx;
std::vector<Entry> g_regs;          // indice = type - kVanillaItemCount
std::atomic<int> g_total{0};           // registrados
std::atomic<int> g_installed{0};      // ja nas tabelas do jogo
int g_poolFirst = -1;
}
namespace bl::runtime {
using namespace item_registry;
int registerModItem(ModItemDef def) {
    std::lock_guard<std::mutex> l(g_mx);
    for (const Entry& r : g_regs) {
        if (r.def.mod == def.mod && r.def.name == def.name) return -1;
    }
    Entry reg;
    reg.def = std::move(def);
    g_regs.push_back(std::move(reg));
    g_total.store(static_cast<int>(g_regs.size()), std::memory_order_release);
    return kVanillaItemCount + static_cast<int>(g_regs.size()) - 1;
}

bool isModItem(int type) {
    return type >= kVanillaItemCount &&
           type < kVanillaItemCount + g_total.load(std::memory_order_acquire);
}

std::string modItemKey(int type) {
    if (!isModItem(type)) return {};
    std::lock_guard<std::mutex> l(g_mx);
    const Entry& e = g_regs[static_cast<size_t>(type - kVanillaItemCount)];
    // O "?" responde pelo item que representa; um "?" livre nao e nada.
    if (isUnloadedType(type)) return e.unloadedKey;
    return e.def.mod + "/" + e.def.name;
}

int modItemTypeByKey(const std::string& key) {
    const size_t slash = key.find('/');
    if (slash == std::string::npos) return -1;
    std::lock_guard<std::mutex> l(g_mx);
    for (size_t i = 0; i < g_regs.size(); ++i) {
        const ModItemDef& d = g_regs[i].def;
        if (isUnloadedType(kVanillaItemCount + static_cast<int>(i))) continue;
        if (key.compare(0, slash, d.mod) == 0 && slash == d.mod.size() &&
            key.compare(slash + 1, std::string::npos, d.name) == 0) {
            return kVanillaItemCount + static_cast<int>(i);
        }
    }
    return -1;
}

bool isUnloadedType(int type) {
    return g_poolFirst >= 0 && type >= g_poolFirst && type < g_poolFirst + kUnloadedPoolSize;
}

}
