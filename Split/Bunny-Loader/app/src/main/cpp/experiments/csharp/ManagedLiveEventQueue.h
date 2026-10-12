#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace bl::csharp {

struct ManagedNpcSnapshot {
    int npcType = -1;
    int npcLife = -1;
    float posX = 0.0f;
    float posY = 0.0f;
    bool valid = false;
};

inline bool managedNpcSnapshotIsValid(const ManagedNpcSnapshot& npc) {
    return npc.valid && npc.npcType >= 0 && npc.npcType <= 1000000 &&
        npc.npcLife > 0 && std::isfinite(npc.posX) && std::isfinite(npc.posY) &&
        npc.posX >= -10000000.0f && npc.posX <= 10000000.0f &&
        npc.posY >= -10000000.0f && npc.posY <= 10000000.0f;
}

struct ManagedPlayerSnapshot {
    int playerIndex = -1;
    int life = -1;
    int lifeMax = -1;
    float posX = 0.0f;
    float posY = 0.0f;
    bool valid = false;
};

inline bool managedPlayerSnapshotIsValid(const ManagedPlayerSnapshot& player) {
    return player.valid && player.playerIndex >= 0 && player.playerIndex < 255 &&
        player.life > 0 && player.life <= 1000000 &&
        player.lifeMax >= player.life && player.lifeMax <= 1000000 &&
        std::isfinite(player.posX) && std::isfinite(player.posY) &&
        player.posX >= -10000000.0f && player.posX <= 10000000.0f &&
        player.posY >= -10000000.0f && player.posY <= 10000000.0f;
}

struct ManagedLiveQueuedEvent {
    int tick = 0;
    int damage = 0;
    bool isKill = false;
    uint64_t enqueuedAtNs = 0;
    int bindingSlot = -1;
    int npcIndex = -1;
    ManagedNpcSnapshot npc{};
    ManagedPlayerSnapshot player{};
};

class ManagedLiveEventQueue {
public:
    static constexpr int kCapacity = 64;
    static constexpr int kEventLimit = 25000;

    bool push(const ManagedLiveQueuedEvent& event) {
        if (count_ == kCapacity) return false;
        data_[(head_ + count_) % kCapacity] = event;
        ++count_;
        return true;
    }

    bool pop(ManagedLiveQueuedEvent* event) {
        if (!event || count_ == 0) return false;
        *event = data_[head_];
        head_ = (head_ + 1) % kCapacity;
        --count_;
        return true;
    }

    void clear() {
        head_ = 0;
        count_ = 0;
    }

    int size() const { return count_; }
    bool empty() const { return count_ == 0; }

private:
    std::array<ManagedLiveQueuedEvent, kCapacity> data_{};
    int head_ = 0;
    int count_ = 0;
};

}
