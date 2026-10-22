#pragma once

#include <atomic>
#include <cstdint>

namespace bl::csharp {

class NativeBiomeUpdateSignal {
public:
    void arm() {
        pending_.store(0, std::memory_order_relaxed);
        armed_.store(true, std::memory_order_release);
    }

    void noteLocalPlayerUpdate() {
        if (!armed_.load(std::memory_order_acquire)) return;
        uint32_t prior = pending_.load(std::memory_order_relaxed);
        while (prior < 64 && !pending_.compare_exchange_weak(
            prior, prior + 1, std::memory_order_release,
            std::memory_order_relaxed)) {}
    }

    uint32_t takePending() {
        if (!armed_.load(std::memory_order_acquire)) return 0;
        return pending_.exchange(0, std::memory_order_acq_rel);
    }

    void pause() {
        armed_.store(false, std::memory_order_release);
        pending_.store(0, std::memory_order_release);
    }

    bool armed() const {
        return armed_.load(std::memory_order_acquire);
    }

private:
    std::atomic<uint32_t> pending_{0};
    std::atomic<bool> armed_{false};
};

} // namespace bl::csharp
