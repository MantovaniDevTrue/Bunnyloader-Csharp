#pragma once

#include "experiments/csharp/NativeHairSlotProbe.h"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace bl::csharp::hair {

class NativeHairSlotMailbox {
public:
    using Reader = std::function<NativeHairSlotSnapshot()>;

    NativeHairSlotSnapshot Request(std::chrono::milliseconds timeout) {
        NativeHairSlotSnapshot rejected;
        if (timeout <= std::chrono::milliseconds::zero() ||
            timeout > std::chrono::seconds(5)) {
            rejected.error = "HairSlotMailboxInvalidTimeout";
            return rejected;
        }
        std::unique_lock lock(mutex_);
        if (pending_) {
            rejected.error = "HairSlotMailboxAlreadyPending";
            return rejected;
        }
        ++generation_;
        pending_ = true;
        completed_ = false;
        awaiting_.store(true, std::memory_order_release);
        if (!ready_.wait_for(lock, timeout, [&] { return completed_ || !pending_; })) {
            pending_ = false;
            ++generation_;
            awaiting_.store(false, std::memory_order_release);
            rejected.error = "HairSlotMailboxTimedOut";
            return rejected;
        }
        if (!completed_) {
            rejected.error = "HairSlotMailboxCancelled";
            return rejected;
        }
        NativeHairSlotSnapshot answer = response_;
        completed_ = false;
        pending_ = false;
        return answer;
    }

    void Pump(const Reader& reader) {
        if (!awaiting_.load(std::memory_order_acquire)) return;
        uint64_t ticket = 0;
        {
            std::lock_guard lock(mutex_);
            if (!pending_ || completed_) return;
            ticket = generation_;
        }
        NativeHairSlotSnapshot data;
        if (reader) data = reader();
        else data.error = "HairSlotMailboxReaderUnavailable";
        {
            std::lock_guard lock(mutex_);
            if (!pending_ || completed_ || ticket != generation_) return;
            response_ = std::move(data);
            completed_ = true;
            awaiting_.store(false, std::memory_order_release);
        }
        ready_.notify_one();
    }

    void Cancel() {
        {
            std::lock_guard lock(mutex_);
            pending_ = false;
            completed_ = false;
            ++generation_;
            awaiting_.store(false, std::memory_order_release);
        }
        ready_.notify_all();
    }

    bool HasPending() const {
        return awaiting_.load(std::memory_order_acquire);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    NativeHairSlotSnapshot response_;
    std::atomic<bool> awaiting_{false};
    bool pending_ = false;
    bool completed_ = false;
    uint64_t generation_ = 0;
};

// Somente a thread de update pode executar o probe IL2CPP. Chamadas do
// worker aguardam no máximo o timeout solicitado, sem polling de FPS.
NativeHairSlotSnapshot RequestNativeHairSlotsOnWorker(
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));
void PumpNativeHairSlotsOnGameThread();
void CancelNativeHairSlotRequests();

} // namespace bl::csharp::hair
