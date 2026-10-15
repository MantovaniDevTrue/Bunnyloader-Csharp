#pragma once

#include "experiments/csharp/ManagedVisualQueue.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace bl::csharp {

class ManagedVisualState {
public:
    static constexpr size_t MaxActive = 128;
    struct Key {
        int player;
        int effect;
        bool operator<(const Key& other) const {
            return player < other.player ||
                (player == other.player && effect < other.effect);
        }
    };
    struct Entry {
        ManagedVisualCommand command;
        uint64_t expiresAt = 0;
    };

    bool apply(const ManagedVisualCommand& command, uint64_t tick) {
        if (command.playerIndex < 0 || command.playerIndex >= 256 ||
            command.effectId < 0 ||
            !std::isfinite(command.x) || !std::isfinite(command.y) ||
            !std::isfinite(command.scale) || !std::isfinite(command.opacity) ||
            command.scale <= 0 || command.opacity < 0 || command.opacity > 1)
            return false;
        const Key key{command.playerIndex, command.effectId};
        if (command.kind == ManagedVisualKind::ClearOverlay) {
            active_.erase(key);
            return true;
        }
        if (command.kind != ManagedVisualKind::PlayerOverlay &&
            command.kind != ManagedVisualKind::EquipmentSprite)
            return false;
        if (command.durationTicks <= 0 || command.durationTicks > 3600)
            return false;
        if (active_.find(key) == active_.end() && active_.size() >= MaxActive)
            return false;
        active_[key] = Entry{command, tick + static_cast<uint64_t>(command.durationTicks)};
        return true;
    }

    void advance(uint64_t tick) {
        for (auto it = active_.begin(); it != active_.end();) {
            if (tick >= it->second.expiresAt)
                it = active_.erase(it);
            else
                ++it;
        }
    }

    void clearPlayer(int playerIndex) {
        for (auto it = active_.begin(); it != active_.end();) {
            if (it->first.player == playerIndex)
                it = active_.erase(it);
            else
                ++it;
        }
    }

    void clear() { active_.clear(); }
    size_t size() const { return active_.size(); }
    using Visitor = void (*)(const ManagedVisualCommand&, void*);
    void visit(int playerIndex, Visitor visitor, void* context, size_t limit=8) const {
        if(!visitor)return;
        size_t count=0;
        auto it=playerIndex<0?active_.begin():active_.lower_bound(Key{playerIndex,0});
        for(;it!=active_.end()&&count<limit;++it) {
            if(playerIndex>=0&&it->first.player!=playerIndex)break;
            visitor(it->second.command,context);++count;
        }
    }

    std::vector<ManagedVisualCommand> snapshot(int playerIndex, size_t limit) const {
        std::vector<ManagedVisualCommand> result;
        if (playerIndex < 0 || playerIndex >= 256 || limit == 0) return result;
        if (limit > 8) limit = 8;
        const auto first = active_.lower_bound(Key{playerIndex, 0});
        for (auto it = first; it != active_.end() &&
             it->first.player == playerIndex && result.size() < limit; ++it)
            result.push_back(it->second.command);
        return result;
    }

    const Entry* find(int playerIndex, int effectId) const {
        auto found = active_.find(Key{playerIndex, effectId});
        return found == active_.end() ? nullptr : &found->second;
    }

private:
    std::map<Key, Entry> active_;
};

}
