#pragma once

#include "experiments/csharp/ManagedDustContract.h"
#include "experiments/csharp/ManagedDustFlagMask.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <mutex>

namespace bl::csharp {

class ManagedDustMailbox {
public:
    static constexpr std::size_t kSlots = 6000;
    static constexpr std::size_t kPhases = 6;
    static constexpr std::size_t kPendingLimit = 128;
    static constexpr std::uint64_t kMaxResultAgeFrames = 3;

    struct Request {
        ManagedDustState state{};
        std::uint64_t session = 0;
        std::uint64_t frame = 0;
        std::int32_t binding = -1;
        std::int32_t phase = 0;
        bool synchronous = false;
    };

    struct Response {
        ManagedDustState state{};
        ManagedDustState before{};
        std::uint32_t flags = 0;
        std::uint64_t session = 0;
        std::uint64_t frame = 0;
        std::int32_t phase = 0;
    };

    bool offer(std::int32_t binding, std::int32_t phase,
               const ManagedDustState& state, std::uint64_t frame,
               bool* wake = nullptr, bool synchronous = false) {
        if (wake) *wake = false;
        if (binding < 0 || phase < 0 || phase >= static_cast<int>(kPhases) ||
            !managedDustStateValid(state) ||
            (phase == 5 ? state.active != 0 : state.active != 1))
            return false;
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock) {
            ++drops_;
            return false;
        }
        const std::size_t slotIndex = static_cast<std::size_t>(state.slot);
        if (phase == 5) {
            if (state.generation < retired_[slotIndex]) return false;
            retired_[slotIndex] = state.generation;
            completed_.notify_all();
            for (std::size_t i = 0; i < kPhases - 1; ++i) {
                Slot& old = slots_[slotIndex * kPhases + i];
                if (old.generation == state.generation &&
                    old.type == state.type && old.binding == binding)
                    old = {};
            }
            requests_.erase(std::remove_if(requests_.begin(), requests_.end(),
                [&](const Request& pending) {
                    return pending.phase != 5 && pending.binding == binding &&
                        managedDustSameInstance(pending.state, state);
                }), requests_.end());
        } else if (state.generation <= retired_[slotIndex]) {
            return false;
        }
        if (requests_.size() >= kPendingLimit &&
            (phase == 5 || phase == 0 || synchronous)) {
            const auto evict = std::find_if(requests_.rbegin(), requests_.rend(),
                [](const Request& queued) { return queued.phase != 5 && queued.phase != 0; });
            if (evict != requests_.rend()) {
                const Request abandoned = *evict;
                Slot& previous = slots_[static_cast<std::size_t>(abandoned.state.slot) *
                    kPhases + static_cast<std::size_t>(abandoned.phase)];
                if (previous.pending && previous.binding == abandoned.binding &&
                    previous.type == abandoned.state.type &&
                    previous.generation == abandoned.state.generation &&
                    previous.frame == abandoned.frame)
                    previous.pending = false;
                requests_.erase(std::next(evict).base());
                ++drops_;
            }
        }
        if (requests_.size() >= kPendingLimit) {
            ++drops_;
            return false;
        }
        Slot& slot = slots_[slotIndex * kPhases + static_cast<size_t>(phase)];
        if (slot.pending && slot.type == state.type &&
            slot.generation == state.generation &&
            frame >= slot.frame && frame - slot.frame <= kMaxResultAgeFrames)
            return false;
        slot = {};
        slot.type = state.type;
        slot.generation = state.generation;
        slot.pending = true;
        slot.binding = binding;
        slot.phase = phase;
        slot.frame = frame;
        const bool wasEmpty = requests_.empty();
        Request request{state, session_, frame, binding, phase, synchronous};
        if (phase == 5) {
            requests_.push_front(request);
        } else if (phase == 0 || synchronous) {
            const auto firstNormal = std::find_if(requests_.begin(), requests_.end(),
                [](const Request& queued) {
                    return queued.phase != 5 && queued.phase != 0;
                });
            requests_.insert(firstNormal, request);
        } else {
            requests_.push_back(request);
        }
        if (wake) *wake = wasEmpty || phase == 5 || phase == 0 || synchronous;
        return true;
    }

    bool current(const Request& request) const {
        if (request.binding < 0 || request.phase < 0 ||
            request.phase >= static_cast<int>(kPhases) ||
            !managedDustStateValid(request.state)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return currentLocked(request);
    }

    std::size_t take(Request* output, std::size_t max) {
        if (!output || max == 0) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        while (count < max && !requests_.empty()) {
            Request request = requests_.front();
            requests_.pop_front();
            if (currentLocked(request))
                output[count++] = request;
        }
        return count;
    }

    bool complete(const Request& request, const ManagedDustState& state,
                  std::uint32_t flags) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request.session != session_ || request.phase < 0 || request.phase > 5 ||
            !managedDustStateValid(request.state) ||
            !currentLocked(request) || !managedDustStateValid(state) ||
            !managedDustSameInstance(request.state, state) ||
            !managedDustFlagsValid(request.phase, flags, state.active != 0))
            return false;
        Slot& slot = slots_[static_cast<std::size_t>(state.slot) * kPhases + static_cast<size_t>(request.phase)];
        if (!slot.pending || slot.type != state.type ||
            slot.generation != state.generation ||
            slot.binding != request.binding ||
            slot.phase != request.phase || slot.frame != request.frame)
            return false;
        slot.pending = false;
        slot.ready = (flags & 1u) != 0u;
        if (slot.ready)
            slot.result = {state, request.state, flags, request.session, request.frame, request.phase};
        if (request.synchronous) completed_.notify_all();
        return true;
    }

    bool await(std::int32_t binding, std::int32_t phase,
               const ManagedDustState& original, std::uint64_t frame,
               std::chrono::steady_clock::time_point deadline,
               Response* out) {
        if (!out || binding < 0 || phase < 0 || phase >= static_cast<int>(kPhases) ||
            !managedDustStateValid(original)) return false;
        std::unique_lock<std::mutex> lock(mutex_);
        Slot& slot = slots_[static_cast<std::size_t>(original.slot) * kPhases +
                            static_cast<std::size_t>(phase)];
        const std::uint64_t session = session_;
        const auto same = [&]() {
            return session_ == session && slot.binding == binding &&
                   slot.phase == phase && slot.type == original.type &&
                   slot.generation == original.generation && slot.frame == frame &&
                   (phase == 5 || original.generation >
                        retired_[static_cast<std::size_t>(original.slot)]);
        };
        completed_.wait_until(lock, deadline, [&]() {
            return !same() || slot.ready || !slot.pending;
        });
        if (!same()) return false;
        if (slot.ready) {
            const Response result = slot.result;
            slot.ready = false;
            if (result.session == session && result.frame == frame &&
                result.phase == phase &&
                managedDustSameInstance(result.before, original)) {
                *out = result;
                return true;
            }
        }
        return false;
    }

    bool consume(std::int32_t binding, std::int32_t phase,
                 std::int32_t slot, std::int32_t type, std::int32_t generation,
                 std::uint64_t now, Response* out) {
        if (!out || binding < 0 || phase < 0 || phase > 5 ||
            (unsigned)slot >= kSlots ||
            generation <= 0) return false;
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock) return false;
        Slot& state = slots_[static_cast<std::size_t>(slot) * kPhases + static_cast<size_t>(phase)];
        if (!state.ready) return false;
        Response result = state.result;
        state.ready = false;
        if (state.binding != binding || result.phase != phase || state.type != type ||
            state.generation != generation ||
            result.session != session_ || now < result.frame ||
            now - result.frame > kMaxResultAgeFrames)
            return false;
        *out = result;
        return true;
    }

    void discard(const Request& request) {
        if (request.phase < 0 || static_cast<size_t>(request.phase) >= kPhases ||
            request.state.slot < 0 || static_cast<size_t>(request.state.slot) >= kSlots)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (request.session != session_) return;
        Slot& slot = slots_[static_cast<size_t>(request.state.slot) * kPhases +
                            static_cast<size_t>(request.phase)];
        if (slot.pending && slot.type == request.state.type &&
            slot.generation == request.state.generation &&
            slot.frame == request.frame && slot.binding == request.binding)
            slot.pending = false;
    }

    void resetWorld() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++session_;
        if (session_ == 0) ++session_;
        requests_.clear();
        slots_.fill({});
        retired_.fill(0);
        completed_.notify_all();
    }

    bool hasQueued() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !requests_.empty();
    }

    std::uint64_t session() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_;
    }

    std::uint64_t drops() const {
        return drops_.load(std::memory_order_relaxed);
    }

private:
    bool currentLocked(const Request& request) const {
        if (request.session != session_ ||
            (request.phase != 5 &&
             request.state.generation <=
                retired_[static_cast<std::size_t>(request.state.slot)]))
            return false;
        const Slot& slot = slots_[static_cast<std::size_t>(request.state.slot) *
            kPhases + static_cast<std::size_t>(request.phase)];
        return slot.pending && slot.binding == request.binding &&
               slot.phase == request.phase && slot.type == request.state.type &&
               slot.generation == request.state.generation &&
               slot.frame == request.frame;
    }

    struct Slot {
        std::int32_t type = -1;
        std::int32_t generation = 0;
        std::int32_t binding = -1;
        std::int32_t phase = -1;
        std::uint64_t frame = 0;
        bool pending = false;
        bool ready = false;
        Response result{};
    };

    mutable std::mutex mutex_;
    std::condition_variable completed_;
    std::uint64_t session_ = 1;
    std::atomic<std::uint64_t> drops_{0};
    std::array<Slot, kSlots * kPhases> slots_{};
    std::array<std::int32_t, kSlots> retired_{};
    std::deque<Request> requests_;
};

}
