#pragma once

#include <map>
#include <optional>
#include <string>

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

}
