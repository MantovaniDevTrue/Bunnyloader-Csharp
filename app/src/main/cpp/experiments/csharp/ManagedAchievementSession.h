#pragma once

#include <cstdint>

namespace bl::csharp {

// Accessed only while the existing MonoVM achievement FIFO mutex is held.
// Old-world jobs carry an epoch and cannot execute after the next activation.
class ManagedAchievementSession {
public:
    std::uint64_t pause() {
        enabled_ = false;
        return ++epoch_;
    }

    std::uint64_t resume() {
        enabled_ = true;
        return epoch_;
    }

    std::uint64_t stamp() const {
        return enabled_ ? epoch_ : 0;
    }

    bool accepts(std::uint64_t generation) const {
        return enabled_ && generation != 0 && generation == epoch_;
    }

    std::uint64_t epoch() const { return epoch_; }
    bool active() const { return enabled_; }

private:
    std::uint64_t epoch_ = 1;
    bool enabled_ = false;
};

}
