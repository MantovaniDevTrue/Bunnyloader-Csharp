#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "content/items/ModPrefixes.h"
#include "core/Log.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <mutex>
#include <set>

namespace bl::csharp {
namespace {

std::mutex g_prefixMutex;
std::map<int, ManagedPrefixEntry> g_descriptors;
std::set<int> g_reservedManagedPrefixTypes;

bool valid(const ManagedPrefixEntry& p) {
    if (p.type < 0 || p.type > 255 || p.category < 0 || p.category > 5 ||
        p.displayName.size() > 80 ||
        std::any_of(p.displayName.begin(), p.displayName.end(),
                    [](unsigned char ch) { return ch < 32; }) ||
        !std::isfinite(p.rollWeight) || p.rollWeight <= 0.f || p.rollWeight > 20.f || std::floor(p.rollWeight) != p.rollWeight ||
        !std::isfinite(p.valueMultiplier) || p.valueMultiplier < 0.05f ||
        p.valueMultiplier > 100.f || p.crit < -100 || p.crit > 100 ||
        p.tagDamage < -100 || p.tagDamage > 100 ||
        p.armorPenetration < -100 || p.armorPenetration > 100 ||
        p.accessoryDefense < -100 || p.accessoryDefense > 100 ||
        (p.category != 5 && p.accessoryDefense != 0)) return false;
    for (float value : p.multipliers)
        if (!std::isfinite(value) || value < 0.1f || value > 10.f) return false;
    return true;
}

}

void reserveManagedPrefixDescriptor(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    if (type > 0 && type <= 255) g_reservedManagedPrefixTypes.insert(type);
}

bool managedPrefixDescriptorPending(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return g_reservedManagedPrefixTypes.contains(type) && !g_descriptors.contains(type);
}

bool managedPrefixDescriptorReady(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return g_descriptors.contains(type);
}

bool applyManagedPrefixDescriptors(
    const std::string& modId, const std::vector<ManagedPrefixEntry>& prefixes) {
    if (prefixes.size() > 16) return false;
    std::map<int, ManagedPrefixEntry> staged;
    for (const auto& entry : prefixes) {
        if (!valid(entry) ||
            bl::runtime::modPrefixByKey(modId + "/" + entry.name) != entry.type ||
            !staged.emplace(entry.type, entry).second) return false;
    }
    if (!staged.empty() && !installManagedPrefixGameplayHooks()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        for (const auto& entry : staged)
            if (g_descriptors.contains(entry.first)) return false;
        g_descriptors.insert(staged.begin(), staged.end());
    }
    BL_INFO("csharp prefix: %zu descritores gerenciados registrados para %s; hooks de gameplay instalados",
            staged.size(), modId.c_str());
    return true;
}

std::vector<ManagedPrefixEntry> snapshotManagedPrefixes() {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    std::vector<ManagedPrefixEntry> items;
    items.reserve(g_descriptors.size());
    for (const auto& pair : g_descriptors) items.push_back(pair.second);
    return items;
}

int registeredManagedPrefixCount() {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return static_cast<int>(g_descriptors.size());
}

}
