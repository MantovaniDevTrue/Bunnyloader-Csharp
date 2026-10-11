#pragma once
#include <array>
#include <cstddef>

namespace bl::csharp {

struct ManagedLiveQueuedEvent {
    int tick = 0;
    int damage = 0;
    bool isKill = false;
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
