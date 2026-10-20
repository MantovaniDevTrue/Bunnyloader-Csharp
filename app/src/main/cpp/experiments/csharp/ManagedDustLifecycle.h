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

    ManagedDustLifecycle() { positions_.fill(-1); }

    void observe(const ManagedDustState& state) {
        if (!managedDustStateValid(state) || state.active == 0) return;
        Slot& tracked = tracked_[static_cast<size_t>(state.slot)];
        if (!tracked.active) {
            tracked.active = true;
            tracked.state = state;
            if (positions_[static_cast<size_t>(state.slot)] < 0) {
                positions_[static_cast<size_t>(state.slot)] =
                    static_cast<int>(indices_.size());
                indices_.push_back(state.slot);
            }
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
        const int index = positions_[static_cast<size_t>(slot)];
        if (index >= 0) eraseTracked(static_cast<size_t>(index));
    }

    template <class Alive>
    void sweep(Alive alive) {
        size_t i = 0;
        while (i < indices_.size()) {
            const int index = indices_[i];
            Slot& tracked = tracked_[static_cast<size_t>(index)];
            if (!tracked.active) {
                eraseTracked(i);
                continue;
            }
            if (!alive(index, tracked.state.type, tracked.state.generation)) {
                queueRetirement(tracked.state);
                tracked.active = false;
                eraseTracked(i);
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
        positions_.fill(-1);
        pending_.clear();
        dropped_ = 0;
    }

    size_t trackedCount() const { return indices_.size(); }
    size_t pendingCount() const { return pending_.size(); }
    uint64_t droppedCount() const { return dropped_; }

private:
    void eraseTracked(size_t index) {
        const int removed = indices_[index];
        const int tail = indices_.back();
        indices_[index] = tail;
        positions_[static_cast<size_t>(tail)] = static_cast<int>(index);
        indices_.pop_back();
        positions_[static_cast<size_t>(removed)] = -1;
    }

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
    std::array<int, kSlots> positions_{};
    std::deque<ManagedDustState> pending_;
    uint64_t dropped_ = 0;
};

}
