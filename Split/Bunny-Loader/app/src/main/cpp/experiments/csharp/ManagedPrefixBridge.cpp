#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "content/items/ModPrefixes.h"
#include "content/items/ModItems.h"
#include "core/Log.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <set>

namespace bl::csharp {
namespace {

std::mutex g_prefixMutex;
std::map<int, ManagedPrefixEntry> g_descriptors;
std::set<int> g_reservedManagedPrefixTypes;
std::shared_ptr<const std::vector<ManagedPrefixEntry>> g_prefixView =
    std::make_shared<const std::vector<ManagedPrefixEntry>>();

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
    if (p.itemWeights.size() + p.itemNames.size() > 32) return false;
    for (const auto& [itemType, weight] : p.itemWeights)
        if (itemType < 1 || itemType > 32767 || weight < 0 || weight > 20) return false;
    for (const auto& [name, weight] : p.itemNames) {
        if (name.empty() || name.size() > 64 || weight < 0 || weight > 20 ||
            !((name[0] >= 'A' && name[0] <= 'Z') ||
              (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) return false;
        for (unsigned char ch : name)
            if (!((ch >= 'A' && ch <= 'Z') ||
                  (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
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
            bl::runtime::modPrefixByKey(modId + "/" + entry.name) != entry.type)
            return false;
        ManagedPrefixEntry resolved = entry;
        for (const auto& [name, weight] : entry.itemNames) {
            const int type = bl::runtime::modItemTypeByKey(modId + "/" + name);
            if (type < 1 || type > 32767 ||
                !resolved.itemWeights.emplace(type, weight).second) return false;
        }
        resolved.itemNames.clear();
        if (!valid(resolved) || !staged.emplace(entry.type, std::move(resolved)).second)
            return false;
    }
    if (!staged.empty() && !installManagedPrefixGameplayHooks()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        for (const auto& entry : staged)
            if (g_descriptors.contains(entry.first)) return false;
        auto next = std::make_shared<std::vector<ManagedPrefixEntry>>();
        next->reserve(g_descriptors.size() + staged.size());
        for (const auto& entry : g_descriptors) next->push_back(entry.second);
        for (const auto& entry : staged) next->push_back(entry.second);
        std::sort(next->begin(), next->end(),
                  [](const auto& a, const auto& b) { return a.type < b.type; });
        std::shared_ptr<const std::vector<ManagedPrefixEntry>> published = next;
        g_descriptors.insert(staged.begin(), staged.end());
        std::atomic_store_explicit(&g_prefixView, std::move(published), std::memory_order_release);
    }
    BL_INFO("csharp prefix: %zu descritores gerenciados registrados para %s; hooks de gameplay instalados",
            staged.size(), modId.c_str());
    return true;
}

std::shared_ptr<const std::vector<ManagedPrefixEntry>> managedPrefixView() {
    return std::atomic_load_explicit(&g_prefixView, std::memory_order_acquire);
}

std::vector<ManagedPrefixEntry> snapshotManagedPrefixes() {
    return *managedPrefixView();
}

int registeredManagedPrefixCount() {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return static_cast<int>(g_descriptors.size());
}

}
