#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace bl::csharp {

class NativeRarityPalette {
public:
    static constexpr int kFirstCustom = 12;
    static constexpr int kMaximumCustom = 4096;

    NativeRarityPalette() {
        for (auto& value : values_) value.store(0, std::memory_order_relaxed);
        for (auto& state : ready_) state.store(false, std::memory_order_relaxed);
    }

    bool publish(int type, uint32_t argb) {
        const int slot = type - kFirstCustom;
        if (slot < 0 || slot >= kMaximumCustom) return false;
        values_[static_cast<size_t>(slot)].store(argb, std::memory_order_relaxed);
        ready_[static_cast<size_t>(slot)].store(true, std::memory_order_release);
        return true;
    }

    bool read(int type, uint32_t* argb) const {
        const int slot = type - kFirstCustom;
        if (!argb || slot < 0 || slot >= kMaximumCustom ||
            !ready_[static_cast<size_t>(slot)].load(std::memory_order_acquire))
            return false;
        *argb = values_[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
        return true;
    }

    bool invalidate(int type) {
        const int slot = type - kFirstCustom;
        if (slot < 0 || slot >= kMaximumCustom) return false;
        ready_[static_cast<size_t>(slot)].store(false, std::memory_order_release);
        return true;
    }

private:
    std::array<std::atomic<uint32_t>, kMaximumCustom> values_;
    std::array<std::atomic<bool>, kMaximumCustom> ready_;
};

}
