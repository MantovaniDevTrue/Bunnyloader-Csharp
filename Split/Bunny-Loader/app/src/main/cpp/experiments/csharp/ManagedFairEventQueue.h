#pragma once

#include "ManagedLiveEventQueue.h"

namespace bl::csharp {

class ManagedFairEventQueue {
public:
    static constexpr int kBindingCount = 32;
    static constexpr int kPerBindingCapacity = 16;

    ManagedFairEventQueue() { clear(); }

    bool push(const ManagedLiveQueuedEvent& event) {
        const int binding = event.bindingSlot + 1;
        if (binding < 0 || binding >= kBindingCount || free_ < 0 ||
            counts_[binding] >= kPerBindingCapacity) return false;
        const int node = free_;
        free_ = nodes_[node].next;
        nodes_[node] = {event, -1};
        if (tails_[binding] < 0) heads_[binding] = node;
        else nodes_[tails_[binding]].next = node;
        tails_[binding] = node;
        ++counts_[binding];
        ++count_;
        return true;
    }

    bool pop(ManagedLiveQueuedEvent* event) {
        if (!event || count_ == 0) return false;
        for (int i = 0; i < kBindingCount; ++i) {
            const int binding = (cursor_ + i) % kBindingCount;
            if (heads_[binding] < 0) continue;
            const int node = heads_[binding];
            *event = nodes_[node].event;
            heads_[binding] = nodes_[node].next;
            if (heads_[binding] < 0) tails_[binding] = -1;
            nodes_[node].next = free_;
            free_ = node;
            --counts_[binding];
            --count_;
            cursor_ = (binding + 1) % kBindingCount;
            return true;
        }
        return false;
    }

    int remove(int bindingSlot) {
        const int binding = bindingSlot + 1;
        if (binding < 0 || binding >= kBindingCount) return 0;
        const int removed = counts_[binding];
        int node = heads_[binding];
        while (node >= 0) {
            const int next = nodes_[node].next;
            nodes_[node].next = free_;
            free_ = node;
            node = next;
        }
        heads_[binding] = tails_[binding] = -1;
        counts_[binding] = 0;
        count_ -= removed;
        return removed;
    }

    void clear() {
        heads_.fill(-1);
        tails_.fill(-1);
        counts_.fill(0);
        for (int i = 0; i < ManagedLiveEventQueue::kCapacity; ++i)
            nodes_[i].next = i + 1 < ManagedLiveEventQueue::kCapacity ? i + 1 : -1;
        free_ = 0;
        count_ = cursor_ = 0;
    }

    int size() const { return count_; }

private:
    struct Node { ManagedLiveQueuedEvent event{}; int next = -1; };
    std::array<Node, ManagedLiveEventQueue::kCapacity> nodes_{};
    std::array<int, kBindingCount> heads_{}, tails_{}, counts_{};
    int free_ = 0;
    int count_ = 0;
    int cursor_ = 0;
};

}
