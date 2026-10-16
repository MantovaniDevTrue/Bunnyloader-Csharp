#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace bl::csharp {
inline thread_local bool universalCanInstall = false;
inline thread_local void (*universalLeaveMono)() = nullptr;
inline thread_local void (*universalResumeMono)() = nullptr;
class UniversalHookWorkerYield {
public:
    UniversalHookWorkerYield() { if (universalLeaveMono) universalLeaveMono(); }
    ~UniversalHookWorkerYield() { if (universalResumeMono) universalResumeMono(); }
};
class UniversalHookMailbox {
public:
    struct Work { int slot; uint64_t ticket; uintptr_t self; int32_t argument = 0; bool int32Mode = false; };
    struct Outcome { bool accepted = false; bool originalRan = false; bool faulted = false; bool timedOut = false; int32_t value = 0; };
    using Clock = std::chrono::steady_clock;

    template<class Original, class Wake>
    Outcome dispatch(int slot, uintptr_t self, Original original, Wake wake,
                     std::chrono::microseconds budget = std::chrono::milliseconds(3),
                     int32_t argument = 0, bool int32Mode = false) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock || state_ != State::Idle) return {};
        work_ = {slot, ++ticket_, self, argument, int32Mode};
        state_ = State::Pending;
        cancelled_ = false;
        originalRequested_ = originalDone_ = originalRan_ = failed_ = false;
        originalValue_ = completedValue_ = 0;
        auto deadline = Clock::now() + budget;
        wake();
        Outcome out{true, false, false};
        for (;;) {
            if (state_ == State::Done) {
                out.originalRan = originalRan_;
                out.faulted = failed_;
                out.value = completedValue_;
                state_ = State::Idle;
                return out;
            }
            if (cancelled_ || Clock::now() >= deadline) {
                out.timedOut = !cancelled_;
                cancelled_ = true;
                out.originalRan = originalRan_;
                out.faulted = true;
                if (state_ == State::Pending) state_ = State::Idle;
                else state_ = State::Abandoned;
                changed_.notify_all();
                return out;
            }
            if (originalRequested_) {
                originalRequested_ = false;
                originalRan_ = true;
                lock.unlock();
                const auto start = Clock::now();
                const bool ok = original();
                const auto elapsed = Clock::now() - start;
                lock.lock();
                deadline += elapsed;
                originalDone_ = true;
                failed_ |= !ok;
                changed_.notify_all();
                continue;
            }
            changed_.wait_until(lock, deadline);
        }
    }

    bool take(Work& work) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Pending) return false;
        state_ = State::Running;
        worker_ = std::this_thread::get_id();
        work = work_;
        return true;
    }
    bool original(int slot, uint64_t ticket) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != State::Running || cancelled_ || originalRan_ ||
            slot != work_.slot || ticket != work_.ticket || worker_ != std::this_thread::get_id())
            return false;
        UniversalHookWorkerYield yield;
        originalRequested_ = true;
        changed_.notify_all();
        changed_.wait(lock, [&] { return originalDone_ || cancelled_; });
        const bool result = originalDone_ && !cancelled_ && !failed_;
        lock.unlock();
        return result;
    }
    bool originalInt32(int slot, uint64_t ticket, int32_t& result) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!work_.int32Mode) return false;
        }
        if (!original(slot, ticket)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        result = originalValue_;
        return true;
    }
    void setOriginalInt32Value(int32_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::Running && originalRan_) originalValue_ = value;
    }
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        changed_.notify_all();
    }
    bool complete(uint64_t ticket, bool success, int32_t result = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ticket != work_.ticket || worker_ != std::this_thread::get_id()) return false;
        const bool accepted = state_ == State::Running && !cancelled_;
        if (state_ == State::Abandoned) state_ = State::Idle;
        else if (state_ == State::Running) {
            failed_ |= !success || cancelled_;
            completedValue_ = result;
            state_ = State::Done;
        }
        changed_.notify_all();
        return accepted;
    }
private:
    enum class State { Idle, Pending, Running, Done, Abandoned };
    std::mutex mutex_;
    std::condition_variable changed_;
    State state_ = State::Idle;
    Work work_{};
    uint64_t ticket_ = 0;
    std::thread::id worker_;
    bool cancelled_ = false, originalRequested_ = false, originalDone_ = false;
    bool originalRan_ = false, failed_ = false;
    int32_t originalValue_ = 0, completedValue_ = 0;
};
inline bool universalHookCallbackIsTerminal(bool accepted, bool success) {
    return accepted && !success;
}
inline UniversalHookMailbox& universalHookMailbox() {
    static UniversalHookMailbox mailbox;
    return mailbox;
}
}
