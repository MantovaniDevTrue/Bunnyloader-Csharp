#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bl::runtime::item_save {

template <typename Pointer, typename State>
std::optional<State> selectTrackedRarity(
    const std::map<Pointer, State>& byItem,
    const std::map<std::string, State>& bySlot,
    Pointer item, const std::string& slot, int type) {
    const auto tracked = byItem.find(item);
    if (tracked != byItem.end() && tracked->second.type == type)
        return tracked->second;

    const auto saved = bySlot.find(slot);
    if (saved == bySlot.end() || saved->second.type != type) return std::nullopt;
    for (const auto& [originalItem, state] : byItem) {
        if (originalItem != item && state.type == type &&
            state.key == saved->second.key && state.origin == slot)
            return std::nullopt;
    }
    State result = saved->second;
    result.origin = slot;
    return result;
}

template <typename Pointer>
struct LiveRarityItem {
    Pointer item;
    std::string slot;
    int type;
    bool resolved;
};

template <typename Pointer, typename State>
void reconcileTrackedRarities(
    const std::map<Pointer, State>& previousItems,
    const std::map<std::string, State>& previousSlots,
    const std::vector<LiveRarityItem<Pointer>>& current,
    std::map<Pointer, State>* nextItems,
    std::map<std::string, State>* nextSlots) {
    if (!nextItems || !nextSlots) return;
    nextItems->clear();
    nextSlots->clear();
    for (const auto& entry : current) {
        if (!entry.item || entry.type <= 0 || entry.resolved) continue;
        auto pending = selectTrackedRarity(
            previousItems, previousSlots, entry.item, entry.slot, entry.type);
        if (!pending) continue;
        pending->origin = entry.slot;
        nextItems->emplace(entry.item, *pending);
        nextSlots->emplace(entry.slot, *pending);
    }
}

}
