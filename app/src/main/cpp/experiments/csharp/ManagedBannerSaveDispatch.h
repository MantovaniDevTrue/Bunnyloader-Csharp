#pragma once

#include <atomic>
#include <cstdint>

namespace bl::csharp::banners {

class SaveDispatch final {
public:
    class NetworkScope final {
    public:
        NetworkScope() { ++networkDepth_; }
        ~NetworkScope() { --networkDepth_; }
        NetworkScope(const NetworkScope&) = delete;
        NetworkScope& operator=(const NetworkScope&) = delete;
    };

    void enterWorld(uint64_t epoch) {
        pending_.store(0, std::memory_order_release);
        epoch_.store(epoch, std::memory_order_release);
    }

    void leaveWorld() {
        epoch_.store(0, std::memory_order_release);
        pending_.store(0, std::memory_order_release);
    }

    bool requestFromWorldSave(bool gameplayReady) {
        if (!gameplayReady || networkDepth_ != 0) return false;
        const uint64_t epoch = epoch_.load(std::memory_order_acquire);
        if (!epoch) return false;
        pending_.store(epoch, std::memory_order_release);
        return true;
    }

    bool consumeOnMainThread(uint64_t worldEpoch) {
        const uint64_t requested = pending_.exchange(0, std::memory_order_acq_rel);
        return requested && requested == worldEpoch &&
            requested == epoch_.load(std::memory_order_acquire);
    }

    uint64_t worldEpoch() const {
        return epoch_.load(std::memory_order_acquire);
    }

    static int networkDepth() { return networkDepth_; }

private:
    inline static thread_local int networkDepth_ = 0;
    std::atomic<uint64_t> epoch_{0};
    std::atomic<uint64_t> pending_{0};
};

inline SaveDispatch& saveDispatch() {
    static SaveDispatch dispatch;
    return dispatch;
}

} // namespace bl::csharp::banners
