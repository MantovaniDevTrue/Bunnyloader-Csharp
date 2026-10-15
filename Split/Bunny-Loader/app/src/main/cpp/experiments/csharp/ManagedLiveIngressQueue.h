#pragma once

#include "ManagedLiveEventQueue.h"
#include "ManagedFairEventQueue.h"
#include "ManagedCounters.h"

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
    static constexpr size_t kBindingCount = ManagedFairEventQueue::kBindingCount;
    struct Admission {
        uint64_t session = 0;
        std::array<uint64_t, kBindingCount> bindings{};
    };

    Admission admission() const {
        Admission result;
        result.session = session_.load(std::memory_order_acquire);
        for (size_t i = 0; i < kBindingCount; ++i)
            result.bindings[i] = bindings_[i].load(std::memory_order_acquire);
        return result;
    }

    ManagedLiveIngressPush push(const Admission& ticket,
                                const ManagedLiveQueuedEvent& event,
                                int projectileType,
                                std::atomic<int>& armedType,
                                const std::atomic<bool>& automatic) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!current(ticket, event.bindingSlot) ||
            armedType.load(std::memory_order_acquire) != projectileType ||
            (event.isKill && !automatic.load(std::memory_order_acquire)) ||
            (!continuous_ && accepted_.load(std::memory_order_relaxed) >= ManagedLiveEventQueue::kEventLimit))
            return {};
        if (!(continuous_ ? fairQueue_.push(event) : queue_.push(event))) {
            addManagedCounter(dropped_);
            addManagedCounter(bindingDrops_[static_cast<size_t>(event.bindingSlot + 1)]);
            return {false, true, sizeLocked()};
        }
        const int depth = sizeLocked();
        depth_.store(depth, std::memory_order_release);
        if (depth > maxDepth_.load(std::memory_order_relaxed))
            maxDepth_.store(depth, std::memory_order_relaxed);
        const int accepted = addManagedCounter(accepted_);
        if (!continuous_ && accepted >= ManagedLiveEventQueue::kEventLimit)
            armedType.store(-1, std::memory_order_release);
        return {true, false, depth};
    }

    ManagedLiveIngressPush pushAdditional(const Admission& ticket,
                                          const ManagedLiveQueuedEvent& event,
                                          int projectileType,
                                          const std::atomic<int>& armedType) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!current(ticket, event.bindingSlot) ||
            armedType.load(std::memory_order_acquire) != projectileType ||
            (!continuous_ && accepted_.load(std::memory_order_relaxed) >= ManagedLiveEventQueue::kEventLimit))
            return {};
        if (!(continuous_ ? fairQueue_.push(event) : queue_.push(event))) {
            addManagedCounter(dropped_);
            addManagedCounter(bindingDrops_[static_cast<size_t>(event.bindingSlot + 1)]);
            return {false, true, sizeLocked()};
        }
        const int depth = sizeLocked();
        depth_.store(depth, std::memory_order_release);
        if (depth > maxDepth_.load(std::memory_order_relaxed))
            maxDepth_.store(depth, std::memory_order_relaxed);
        addManagedCounter(accepted_);
        return {true, false, depth};
    }

    bool pop(ManagedLiveQueuedEvent* event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!(continuous_ ? fairQueue_.pop(event) : queue_.pop(event))) return false;
        depth_.store(sizeLocked(), std::memory_order_release);
        return true;
    }

    int clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        session_.fetch_add(1, std::memory_order_release);
        const int pending = sizeLocked();
        queue_.clear();
        fairQueue_.clear();
        depth_.store(0, std::memory_order_release);
        return pending;
    }

    int pauseBinding(int bindingSlot, std::atomic<int>& armedType) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (bindingSlot < -1 || bindingSlot >= static_cast<int>(kBindingCount) - 1)
            return 0;
        bindings_[static_cast<size_t>(bindingSlot + 1)].fetch_add(
            1, std::memory_order_release);
        armedType.store(-1, std::memory_order_release);
        if (continuous_) {
            const int removed = fairQueue_.remove(bindingSlot);
            depth_.store(fairQueue_.size(), std::memory_order_release);
            return removed;
        }
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

    void reset(bool continuous = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        session_.fetch_add(1, std::memory_order_release);
        queue_.clear();
        fairQueue_.clear();
        continuous_ = continuous;
        for (auto& count : bindingDrops_) count.store(0, std::memory_order_relaxed);
        depth_.store(0, std::memory_order_release);
        maxDepth_.store(0, std::memory_order_relaxed);
        accepted_.store(0, std::memory_order_relaxed);
        dropped_.store(0, std::memory_order_relaxed);
    }

    int depth() const { return depth_.load(std::memory_order_acquire); }
    int maxDepth() const { return maxDepth_.load(std::memory_order_relaxed); }
    int accepted() const { return accepted_.load(std::memory_order_relaxed); }
    int dropped() const { return dropped_.load(std::memory_order_relaxed); }
    int bindingDrops(int slot) const {
        if (slot < -1 || slot >= static_cast<int>(kBindingCount) - 1) return 0;
        return bindingDrops_[static_cast<size_t>(slot + 1)].load(std::memory_order_relaxed);
    }

private:
    int sizeLocked() const { return continuous_ ? fairQueue_.size() : queue_.size(); }
    bool current(const Admission& ticket, int bindingSlot) const {
        if (bindingSlot < -1 || bindingSlot >= static_cast<int>(kBindingCount) - 1)
            return false;
        const auto index = static_cast<size_t>(bindingSlot + 1);
        return ticket.session == session_.load(std::memory_order_acquire) &&
            ticket.bindings[index] == bindings_[index].load(std::memory_order_acquire);
    }
    std::atomic<uint64_t> session_{0};
    std::array<std::atomic<uint64_t>, kBindingCount> bindings_{};
    std::mutex mutex_;
    ManagedLiveEventQueue queue_;
    ManagedFairEventQueue fairQueue_;
    bool continuous_ = false;
    std::array<std::atomic<int>, kBindingCount> bindingDrops_{};
    std::atomic<int> depth_{0};
    std::atomic<int> maxDepth_{0};
    std::atomic<int> accepted_{0};
    std::atomic<int> dropped_{0};
};

}

