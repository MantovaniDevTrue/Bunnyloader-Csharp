#pragma once
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace bl::csharp {
inline std::atomic<bool(*)()> universalNativeService{nullptr};
inline thread_local bool universalNativeAccessInProgress=false;
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
    struct Work { int slot; uint64_t ticket; uintptr_t self; int32_t argument = 0; bool int32Mode = false; int32_t argument2 = 0; bool int32PairMode = false; int32_t argument3 = 0; int extendedMode = 0; };
    struct Outcome { bool accepted = false; bool originalRan = false; bool faulted = false; bool timedOut = false; bool retryable = false; int32_t value = 0; };
    using Clock = std::chrono::steady_clock;

    template<class Original, class Wake>
    Outcome dispatch(int slot, uintptr_t self, Original original, Wake wake,
                     std::chrono::microseconds budget = std::chrono::milliseconds(3),
                     int32_t argument = 0, bool int32Mode = false, bool(*service)()=nullptr,
                     int32_t argument2 = 0, bool int32PairMode = false, int32_t argument3 = 0, int extendedMode = 0) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock || state_ != State::Idle) return {};
        work_ = {slot, ++ticket_, self, argument, int32Mode, argument2, int32PairMode, argument3, extendedMode};
        state_ = State::Pending;
        cancelled_ = false;
        originalRequested_ = originalDone_ = originalRan_ = failed_ = retryable_ = false;
        originalValue_ = completedValue_ = 0;
        auto deadline = Clock::now() + budget;
        deadline_ = deadline;
        wake();
        Outcome out{true, false, false};
        for (;;) {
            if (state_ == State::Done) {
                out.originalRan = originalRan_;
                out.faulted = failed_;
                out.retryable = retryable_;
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
                deadline_ = deadline;
                originalDone_ = true;
                failed_ |= !ok;
                changed_.notify_all();
                continue;
            }
            if(service) {
                const uint64_t notifications=notifications_;
                lock.unlock();
                const bool handled=service();
                lock.lock();
                if(handled||originalRequested_||state_==State::Done||cancelled_||notifications_!=notifications)continue;
            }
            changed_.wait_until(lock, deadline);
        }
    }

    void notify() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++notifications_;
        changed_.notify_all();
    }
    bool context(int slot,uint64_t ticket,uintptr_t* self) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!self||state_!=State::Running||cancelled_||slot!=work_.slot||ticket!=work_.ticket)return false;
        *self=work_.self;return true;
    }
    bool workerDeadline(int slot, uint64_t ticket, uintptr_t* self, Clock::time_point* deadline) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!self || !deadline || state_ != State::Running || cancelled_ ||
            slot != work_.slot || ticket != work_.ticket || worker_ != std::this_thread::get_id()) return false;
        *self = work_.self;
        *deadline = deadline_;
        return true;
    }
    bool workerContext(int slot,uint64_t ticket,uintptr_t* self) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!self||state_!=State::Running||cancelled_||slot!=work_.slot||ticket!=work_.ticket||worker_!=std::this_thread::get_id())return false;
        *self=work_.self;return true;
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
            if (!work_.int32Mode && !work_.int32PairMode && work_.extendedMode != 1 && work_.extendedMode != 4) return false;
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
    bool complete(uint64_t ticket, bool success, int32_t result = 0, bool retryable = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ticket != work_.ticket || worker_ != std::this_thread::get_id()) return false;
        const bool accepted = state_ == State::Running && !cancelled_;
        if (state_ == State::Abandoned) state_ = State::Idle;
        else if (state_ == State::Running) {
            failed_ |= !success || cancelled_;
            retryable_ = !success && retryable && !cancelled_;
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
    uint64_t ticket_ = 0, notifications_ = 0;
    std::thread::id worker_;
    bool cancelled_ = false, originalRequested_ = false, originalDone_ = false;
    bool originalRan_ = false, failed_ = false, retryable_ = false;
    Clock::time_point deadline_{};
    int32_t originalValue_ = 0, completedValue_ = 0;
};
inline bool universalHookCallbackIsTerminal(bool accepted, bool success, bool retryable = false) {
    return accepted && !success && !retryable;
}
inline UniversalHookMailbox& universalHookMailbox() {
    static UniversalHookMailbox mailbox;
    return mailbox;
}
}
