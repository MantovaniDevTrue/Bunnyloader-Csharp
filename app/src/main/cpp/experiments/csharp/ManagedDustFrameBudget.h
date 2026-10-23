#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace bl::csharp {

class ManagedDustFrameBudget {
public:
    static constexpr auto kPerFrame = std::chrono::microseconds(600);
    static constexpr auto kPerCallback = std::chrono::microseconds(200);

    bool deadline(std::uint64_t frame, std::chrono::steady_clock::time_point* limit) {
        if (!limit) return false;
        if (frame != frame_) {
            frame_ = frame;
            spent_ = std::chrono::steady_clock::duration::zero();
        }
        if (spent_ >= kPerFrame) return false;
        started_ = std::chrono::steady_clock::now();
        *limit = started_ + std::min(
            std::chrono::duration_cast<std::chrono::microseconds>(kPerFrame - spent_),
            kPerCallback);
        return true;
    }

    void finish() {
        const auto elapsed = std::chrono::steady_clock::now() - started_;
        if (elapsed > std::chrono::steady_clock::duration::zero())
            spent_ += elapsed;
    }

    void reset() {
        frame_ = UINT64_MAX;
        spent_ = std::chrono::steady_clock::duration::zero();
    }

private:
    std::uint64_t frame_ = UINT64_MAX;
    std::chrono::steady_clock::duration spent_{};
    std::chrono::steady_clock::time_point started_{};
};

}
