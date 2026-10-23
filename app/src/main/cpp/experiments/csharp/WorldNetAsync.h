#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bl::csharp::worldsave {
class WorldNetAsync {
public:
    enum class Kind : uint8_t { Send, Receive };
    struct Job {
        Kind kind = Kind::Send;
        uint64_t epoch = 0;
        uint64_t generation = 0;
        int remote = -1;
        int ignore = -1;
        std::vector<uint8_t> packet;
    };
    struct Result {
        Job job;
        bool success = false;
        std::string error;
    };
    using Worker = std::function<bool(const Job&, std::vector<uint8_t>*, std::string*)>;
    static constexpr size_t Capacity = 32;
    static constexpr size_t MaxPerFrame = 4;

    explicit WorldNetAsync(Worker worker)
        : worker_(std::move(worker)), thread_([this] { run(); }) {}
    WorldNetAsync(const WorldNetAsync&) = delete;
    WorldNetAsync& operator=(const WorldNetAsync&) = delete;
    ~WorldNetAsync() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            ++generation_;
            pending_.clear();
            ready_.clear();
        }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    bool send(uint64_t epoch, int remote, int ignore) {
        if (!epoch || remote < -1 || remote >= 256 ||
            ignore < -1 || ignore >= 256) return false;
        Job job;
        job.kind = Kind::Send;
        job.epoch = epoch;
        job.remote = remote;
        job.ignore = ignore;
        return enqueue(std::move(job));
    }

    bool receive(uint64_t epoch, const uint8_t* packet, size_t length) {
        if (!epoch || !packet || length < 14 || length > 60000) return false;
        Job job;
        job.kind = Kind::Receive;
        job.epoch = epoch;
        job.packet.assign(packet, packet + length);
        return enqueue(std::move(job));
    }

    void invalidate() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        outstanding_ = 0;
        pending_.clear();
        ready_.clear();
    }

    template<typename Callback>
    void pump(uint64_t epoch, Callback callback) {
        for (size_t i = 0; i < MaxPerFrame; ++i) {
            Result result;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (ready_.empty()) return;
                result = std::move(ready_.front());
                ready_.pop_front();
                if (result.job.generation == generation_ && outstanding_ > 0)
                    --outstanding_;
                if (result.job.generation != generation_ ||
                    result.job.epoch != epoch) continue;
            }
            callback(result);
        }
    }

private:
    bool enqueue(Job job) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_ || outstanding_ >= Capacity) return false;
        job.generation = generation_;
        pending_.push_back(std::move(job));
        ++outstanding_;
        wake_.notify_one();
        return true;
    }

    void run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this] { return stopped_ || !pending_.empty(); });
                if (stopped_) return;
                job = std::move(pending_.front());
                pending_.pop_front();
                if (job.generation != generation_) continue;
            }
            Result result;
            result.job = std::move(job);
            result.success = worker_(result.job, &result.job.packet, &result.error);
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_ || result.job.generation != generation_) continue;
            ready_.push_back(std::move(result));
        }
    }

    Worker worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> pending_;
    std::deque<Result> ready_;
    size_t outstanding_ = 0;
    uint64_t generation_ = 1;
    bool stopped_ = false;
    std::thread thread_;
};
}
