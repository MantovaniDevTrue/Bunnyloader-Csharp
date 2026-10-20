#pragma once

#include "experiments/csharp/ManagedDustContract.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace bl::csharp {

class ManagedDustLifecycle {
public:
    static constexpr std::size_t kSlots = 6000;
    static constexpr std::size_t kReleaseLimit = 12000;

    void observe(const ManagedDustState& state) {
        if (!managedDustStateValid(state) || state.active == 0) return;
        Slot& tracked = tracked_[static_cast<size_t>(state.slot)];
        if (!tracked.active) {
            tracked.active = true;
            tracked.state = state;
            indices_.push_back(state.slot);
        } else if (!managedDustSameInstance(tracked.state, state)) {
            queueRetirement(tracked.state);
            tracked.state = state;
        } else {
            tracked.state = state;
        }
    }

    void retire(int slot) {
        if (slot < 0 || static_cast<size_t>(slot) >= kSlots) return;
        Slot& tracked = tracked_[static_cast<size_t>(slot)];
        if (!tracked.active) return;
        queueRetirement(tracked.state);
        tracked.active = false;
    }

    template <class Alive>
    void sweep(Alive alive) {
        size_t i = 0;
        while (i < indices_.size()) {
            const int index = indices_[i];
            Slot& tracked = tracked_[static_cast<size_t>(index)];
            if (!tracked.active) {
                indices_[i] = indices_.back();
                indices_.pop_back();
                continue;
            }
            if (!alive(index, tracked.state.type, tracked.state.generation)) {
                queueRetirement(tracked.state);
                tracked.active = false;
                indices_[i] = indices_.back();
                indices_.pop_back();
                continue;
            }
            ++i;
        }
    }

    template <class Send>
    size_t flush(size_t budget, Send send) {
        size_t count = 0;
        while (count < budget && !pending_.empty()) {
            if (!send(pending_.front())) break;
            pending_.pop_front();
            ++count;
        }
        return count;
    }

    void reset() {
        tracked_.fill({});
        indices_.clear();
        pending_.clear();
        dropped_ = 0;
    }

    size_t trackedCount() const { return indices_.size(); }
    size_t pendingCount() const { return pending_.size(); }
    uint64_t droppedCount() const { return dropped_; }

private:
    struct Slot {
        ManagedDustState state{};
        bool active = false;
    };

    void queueRetirement(ManagedDustState state) {
        state.active = 0;
        if (pending_.size() == kReleaseLimit) {
            ++dropped_;
            return;
        }
        pending_.push_back(state);
    }

    std::array<Slot, kSlots> tracked_{};
    std::vector<int> indices_;
    std::deque<ManagedDustState> pending_;
    uint64_t dropped_ = 0;
};

}
