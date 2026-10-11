#pragma once

#include "ManagedLiveEventQueue.h"

#include <atomic>
#include <mutex>

namespace bl::csharp {

struct ManagedLiveIngressPush {
    bool accepted = false;
    bool queueDropped = false;
    int depth = 0;
};

class ManagedLiveIngressQueue {
public:
    ManagedLiveIngressPush push(const ManagedLiveQueuedEvent& event,
                                int projectileType,
                                std::atomic<int>& armedType,
                                const std::atomic<bool>& automatic) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (armedType.load(std::memory_order_acquire) != projectileType ||
            (event.isKill && !automatic.load(std::memory_order_acquire)) ||
            accepted_.load(std::memory_order_relaxed) >= ManagedLiveEventQueue::kEventLimit)
            return {};
        if (!queue_.push(event)) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return {false, true, queue_.size()};
        }
        const int depth = queue_.size();
        depth_.store(depth, std::memory_order_release);
        if (depth > maxDepth_.load(std::memory_order_relaxed))
            maxDepth_.store(depth, std::memory_order_relaxed);
        const int accepted = accepted_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (accepted >= ManagedLiveEventQueue::kEventLimit)
            armedType.store(-1, std::memory_order_release);
        return {true, false, depth};
    }

    ManagedLiveIngressPush pushAdditional(const ManagedLiveQueuedEvent& event,
                                          int projectileType,
                                          const std::atomic<int>& armedType) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (armedType.load(std::memory_order_acquire) != projectileType ||
            accepted_.load(std::memory_order_relaxed) >= ManagedLiveEventQueue::kEventLimit)
            return {};
        if (!queue_.push(event)) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return {false, true, queue_.size()};
        }
        const int depth = queue_.size();
        depth_.store(depth, std::memory_order_release);
        if (depth > maxDepth_.load(std::memory_order_relaxed))
            maxDepth_.store(depth, std::memory_order_relaxed);
        accepted_.fetch_add(1, std::memory_order_relaxed);
        return {true, false, depth};
    }

    bool pop(ManagedLiveQueuedEvent* event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!queue_.pop(event)) return false;
        depth_.store(queue_.size(), std::memory_order_release);
        return true;
    }

    int clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        const int pending = queue_.size();
        queue_.clear();
        depth_.store(0, std::memory_order_release);
        return pending;
    }

    int discardBinding(int bindingSlot) {
        std::lock_guard<std::mutex> lock(mutex_);
        ManagedLiveEventQueue retained;
        ManagedLiveQueuedEvent next;
        int removed = 0;
        while (queue_.pop(&next)) {
            if (next.bindingSlot == bindingSlot) {
                ++removed;
            } else {
                retained.push(next);
            }
        }
        queue_ = retained;
        depth_.store(queue_.size(), std::memory_order_release);
        return removed;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        depth_.store(0, std::memory_order_release);
        maxDepth_.store(0, std::memory_order_relaxed);
        accepted_.store(0, std::memory_order_relaxed);
        dropped_.store(0, std::memory_order_relaxed);
    }

    int depth() const { return depth_.load(std::memory_order_acquire); }
    int maxDepth() const { return maxDepth_.load(std::memory_order_relaxed); }
    int accepted() const { return accepted_.load(std::memory_order_relaxed); }
    int dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    std::mutex mutex_;
    ManagedLiveEventQueue queue_;
    std::atomic<int> depth_{0};
    std::atomic<int> maxDepth_{0};
    std::atomic<int> accepted_{0};
    std::atomic<int> dropped_{0};
};

}
