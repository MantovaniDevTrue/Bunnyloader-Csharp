#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace bl::csharp {

struct ManagedNpcBuffCommand {
    int npcIndex = -1;
    int npcType = -1;
    float npcX = 0.0f;
    float npcY = 0.0f;
    int buffId = 0;
    int durationTicks = 0;
    uint64_t generation = 0;
    int64_t createdAtMs = 0;
    int impulseDirection = 0;
    int impulseTenths = 0;
    int ownerBinding = -2;
    uint64_t ownerGeneration = 0;
};

template <size_t N>
inline bool managedNpcBuffOwnerCurrent(
    const ManagedNpcBuffCommand& request,
    const std::array<std::atomic<uint64_t>, N>& ownerGenerations) {
    if (request.ownerBinding < -1 ||
        static_cast<size_t>(request.ownerBinding + 1) >= N) return false;
    return request.ownerGeneration ==
        ownerGenerations[static_cast<size_t>(request.ownerBinding + 1)].load(
            std::memory_order_acquire);
}

inline bool decodeManagedNpcBuffCode(int value, int* buffId, int* durationTicks) {
    if (!buffId || !durationTicks || value < 1000000 || value >= 2000000) return false;
    const int code = value - 1000000;
    const int buff = code / 1000;
    const int duration = code % 1000;
    if (!(buff == 24 || buff == 39 || buff == 44 || buff == 69) ||
        duration < 30 || duration > 600) return false;
    *buffId = buff;
    *durationTicks = duration;
    return true;
}

inline bool decodeManagedNpcImpulseCode(int value, int* direction, int* tenths) {
    if (!direction || !tenths || value < 2000000 || value >= 2001000)
        return false;
    const int encoded = value - 2000000;
    const int dir = encoded / 100;
    const int strength = encoded % 100;
    if (!(dir == 1 || dir == 2) || strength < 10 || strength > 60)
        return false;
    *direction = dir;
    *tenths = strength;
    return true;
}

struct ManagedNpcBuffEffectStatus {
    bool gameHookInstalled = false;
    int requested = 0;
    int applied = 0;
    int rejected = 0;
    int stale = 0;
    int queueDrops = 0;
    int queueDepth = 0;
    int lastBuffId = 0;
    int lastDurationTicks = 0;
    int lastNpcIndex = -1;
    int impulsesApplied = 0;
    int lastImpulseDirection = 0;
    int lastImpulseTenths = 0;
};

class ManagedNpcBuffEffectQueue {
public:
    static constexpr unsigned kCapacity = 64;

    bool push(const ManagedNpcBuffCommand& command) {
        const unsigned write = write_.load(std::memory_order_relaxed);
        if (write - read_.load(std::memory_order_acquire) >= kCapacity) return false;
        items_[write % kCapacity] = command;
        write_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool peek(ManagedNpcBuffCommand* command) const {
        if (!command) return false;
        const unsigned read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;
        *command = items_[read % kCapacity];
        return true;
    }

    void pop() {
        const unsigned read = read_.load(std::memory_order_relaxed);
        if (read != write_.load(std::memory_order_acquire))
            read_.store(read + 1, std::memory_order_release);
    }

    int depth() const {
        return static_cast<int>(write_.load(std::memory_order_acquire) -
                                read_.load(std::memory_order_acquire));
    }

private:
    std::array<ManagedNpcBuffCommand, kCapacity> items_{};
    std::atomic<unsigned> read_{0};
    std::atomic<unsigned> write_{0};
};

}
