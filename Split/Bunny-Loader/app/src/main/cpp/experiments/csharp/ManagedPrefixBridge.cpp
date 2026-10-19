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
std::shared_ptr<const std::map<int, bool>> g_itemChanceView =
    std::make_shared<const std::map<int, bool>>();

bool valid(const ManagedPrefixEntry& p) {
    if (p.type < 0 || p.type > 255 || p.category < 0 || p.category > 6 ||
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
    for (const auto& [type, allowed] : p.itemCanRoll)
        if (!p.itemWeights.contains(type)) return false;
    for (const auto& [name, allowed] : p.itemNamesCanRoll)
        if (!p.itemNames.contains(name)) return false;
    for (int type : p.preferredTypes) {
        const auto weight = p.itemWeights.find(type);
        const auto decision = p.itemCanRoll.find(type);
        if (weight == p.itemWeights.end() ||
            !(decision == p.itemCanRoll.end() ? weight->second > 0 : decision->second))
            return false;
    }
    for (const auto& name : p.preferredNames) {
        const auto weight = p.itemNames.find(name);
        const auto decision = p.itemNamesCanRoll.find(name);
        if (weight == p.itemNames.end() ||
            !(decision == p.itemNamesCanRoll.end() ? weight->second > 0 : decision->second))
            return false;
    }
    if (p.category == 6) {
        bool anyAllowed = false;
        for (const auto& [type, weight] : p.itemWeights) {
            const auto override = p.itemCanRoll.find(type);
            anyAllowed |= override == p.itemCanRoll.end() ? weight > 0 : override->second;
        }
        for (const auto& [name, weight] : p.itemNames) {
            const auto override = p.itemNamesCanRoll.find(name);
            anyAllowed |= override == p.itemNamesCanRoll.end() ? weight > 0 : override->second;
        }
        if (!anyAllowed) return false;
    }
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

bool applyManagedItemPrefixChances(const std::vector<std::pair<int, bool>>& policies) {
    if (policies.size() > 4096) return false;
    std::map<int, bool> staged;
    for (const auto& [type, allowed] : policies)
        if (type < 1 || type > 32767 || !staged.emplace(type, allowed).second)
            return false;
    if (staged.empty()) return true;
    if (!installManagedPrefixChanceHook()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        auto previous = std::atomic_load_explicit(&g_itemChanceView, std::memory_order_acquire);
        for (const auto& [type, allowed] : staged)
            if (previous->contains(type)) return false;
        auto updated = std::make_shared<std::map<int, bool>>(*previous);
        updated->insert(staged.begin(), staged.end());
        std::shared_ptr<const std::map<int, bool>> published = updated;
        std::atomic_store_explicit(&g_itemChanceView, std::move(published), std::memory_order_release);
    }
    return true;
}

std::shared_ptr<const std::map<int, bool>> managedItemPrefixChanceView() {
    return std::atomic_load_explicit(&g_itemChanceView, std::memory_order_acquire);
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
            const auto decision = entry.itemNamesCanRoll.find(name);
            if (decision != entry.itemNamesCanRoll.end() &&
                !resolved.itemCanRoll.emplace(type, decision->second).second) return false;
            if (entry.preferredNames.contains(name) && !resolved.preferredTypes.insert(type).second)
                return false;
        }
        resolved.itemNames.clear();
        resolved.itemNamesCanRoll.clear();
        resolved.preferredNames.clear();
        if (!valid(resolved) || !staged.emplace(entry.type, std::move(resolved)).second)
            return false;
    }
    if (!staged.empty() && !installManagedPrefixGameplayHooks()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        for (const auto& entry : staged)
            if (g_descriptors.contains(entry.first)) return false;
        std::set<int> chosenItems;
        for (const auto& entry : g_descriptors)
            for (int item : entry.second.preferredTypes)
                chosenItems.insert(item);
        for (const auto& entry : staged)
            for (int item : entry.second.preferredTypes)
                if (!chosenItems.insert(item).second) return false;
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
