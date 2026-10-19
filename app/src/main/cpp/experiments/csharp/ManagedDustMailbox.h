#pragma once

#include "experiments/csharp/ManagedDustContract.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace bl::csharp {

class ManagedDustMailbox {
public:
    static constexpr std::size_t kSlots = 6000;
    static constexpr std::size_t kPendingLimit = 128;
    static constexpr std::uint64_t kMaxResultAgeFrames = 3;

    struct Request {
        ManagedDustState state{};
        std::uint64_t session = 0;
        std::uint64_t frame = 0;
        std::int32_t binding = -1;
    };

    struct Response {
        ManagedDustState state{};
        std::uint32_t flags = 0;
        std::uint64_t session = 0;
        std::uint64_t frame = 0;
    };

    bool offer(std::int32_t binding, const ManagedDustState& state,
               std::uint64_t frame) {
        if (binding < 0 || !managedDustStateValid(state)) return false;
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock || requests_.size() >= kPendingLimit) {
            ++drops_;
            return false;
        }
        Slot& slot = slots_[static_cast<std::size_t>(state.slot)];
        if (slot.pending && slot.type == state.type &&
            slot.generation == state.generation) return false;
        slot = {};
        slot.type = state.type;
        slot.generation = state.generation;
        slot.pending = true;
        slot.binding = binding;
        requests_.push_back({state, session_, frame, binding});
        return true;
    }

    std::size_t take(Request* output, std::size_t max) {
        if (!output || max == 0) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        while (count < max && !requests_.empty()) {
            output[count++] = requests_.front();
            requests_.pop_front();
        }
        return count;
    }

    bool complete(const Request& request, const ManagedDustState& state,
                  std::uint32_t flags) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request.session != session_ ||
            !managedDustStateValid(state) ||
            !managedDustSameInstance(request.state, state) ||
            (flags & ~63u) != 0u) return false;
        Slot& slot = slots_[static_cast<std::size_t>(state.slot)];
        if (!slot.pending || slot.type != state.type ||
            slot.generation != state.generation ||
            slot.binding != request.binding) return false;
        slot.pending = false;
        slot.ready = (flags & 1u) != 0u;
        if (slot.ready)
            slot.result = {state, flags, request.session, request.frame};
        return true;
    }

    bool consume(std::int32_t binding, std::int32_t slot,
                 std::int32_t type, std::int32_t generation,
                 std::uint64_t now, Response* out) {
        if (!out || binding < 0 || (unsigned)slot >= kSlots ||
            generation <= 0) return false;
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock) return false;
        Slot& state = slots_[static_cast<std::size_t>(slot)];
        if (!state.ready) return false;
        Response result = state.result;
        state.ready = false;
        if (state.binding != binding || state.type != type ||
            state.generation != generation ||
            result.session != session_ || now < result.frame ||
            now - result.frame > kMaxResultAgeFrames)
            return false;
        *out = result;
        return true;
    }

    void resetWorld() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++session_;
        if (session_ == 0) ++session_;
        requests_.clear();
        slots_.fill({});
    }

    std::uint64_t session() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_;
    }

    std::uint64_t drops() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return drops_;
    }

private:
    struct Slot {
        std::int32_t type = -1;
        std::int32_t generation = 0;
        std::int32_t binding = -1;
        bool pending = false;
        bool ready = false;
        Response result{};
    };

    mutable std::mutex mutex_;
    std::uint64_t session_ = 1;
    std::uint64_t drops_ = 0;
    std::array<Slot, kSlots> slots_{};
    std::deque<Request> requests_;
};

}
