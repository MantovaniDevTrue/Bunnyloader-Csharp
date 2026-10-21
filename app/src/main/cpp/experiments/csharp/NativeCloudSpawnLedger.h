#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace bl::csharp {

// One Cloud.addCloud hook can produce at most one mod cloud. Its OnSpawn
// callback is handed to MonoVM asynchronously, but the native game thread
// remains the only writer to Terraria.Cloud. A reused array slot invalidates
// the old callback; this prevents a different cloud being flipped by mistake.
class NativeCloudSpawnLedger {
public:
    static constexpr int kCloudSlots = 200;
    static constexpr std::size_t kMaxPending = 128;

    struct Event {
        int type = -1;
        int slot = -1;
        uint32_t generation = 0;
        float scale = 1.f;
        int spriteEffects = 0;
    };

    bool push(int type, int slot, float scale, int spriteEffects,
              Event* queued = nullptr) {
        if (type < 0 || slot < 0 || slot >= kCloudSlots ||
            !std::isfinite(scale) || scale <= 0.f || scale > 10.f ||
            (spriteEffects & ~3) != 0 || count_ >= kMaxPending)
            return false;

        uint32_t& generation = generations_[static_cast<std::size_t>(slot)];
        ++generation;
        if (generation == 0) ++generation;
        Event event{type, slot, generation, scale, spriteEffects};
        data_[(first_ + count_) % kMaxPending] = event;
        ++count_;
        if (queued) *queued = event;
        return true;
    }

    bool take(Event* event) {
        if (!event || count_ == 0) return false;
        *event = data_[first_];
        first_ = (first_ + 1) % kMaxPending;
        --count_;
        return true;
    }

    // Check every condition BEFORE applying a MonoVM result to IL2CPP memory.
    // A previous cloud slot is unusable once another cloud is spawned there.
    bool stillCurrent(const Event& event, bool active, int actualType,
                      float actualScale, int resultingEffects) const {
        if (event.type < 0 || event.slot < 0 || event.slot >= kCloudSlots ||
            event.generation == 0 || !active || actualType != event.type ||
            !std::isfinite(actualScale) || (resultingEffects & ~3) != 0)
            return false;
        return generations_[static_cast<std::size_t>(event.slot)] ==
                   event.generation &&
               std::fabs(event.scale - actualScale) <= 0.0001f;
    }

    // Invalidate when Terraria clears a slot or when a world is unloaded.
    // This operation also prevents any worker result for its previous owner.
    void invalidate(int slot) {
        if (slot < 0 || slot >= kCloudSlots) return;
        uint32_t& generation = generations_[static_cast<std::size_t>(slot)];
        ++generation;
        if (generation == 0) ++generation;
    }

    void clear() {
        first_ = 0;
        count_ = 0;
        generations_.fill(0);
    }

    std::size_t pending() const { return count_; }

private:
    std::array<Event, kMaxPending> data_{};
    std::array<uint32_t, kCloudSlots> generations_{};
    std::size_t first_ = 0;
    std::size_t count_ = 0;
};

} // namespace bl::csharp
