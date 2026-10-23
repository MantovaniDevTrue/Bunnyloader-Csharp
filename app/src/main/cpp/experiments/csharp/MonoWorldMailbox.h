#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace bl::csharp::worldsave {
enum class MonoWorldAction : uint8_t { Register = 1, Enter, Prepare, Confirm, Leave, NetSend, NetReceive };
struct MonoWorldRequest {
    MonoWorldAction action = MonoWorldAction::Register;
    uint64_t epoch = 0;
    std::string modUid, assemblyPath, payload;
    bool authoritative = false;
};
struct MonoWorldReply { bool success = false; std::string payload, error; };
struct MonoWorldTicket {
    MonoWorldRequest request;
    uint64_t generation = 0;
    std::mutex mutex;
    std::condition_variable wake;
    bool done = false, cancelled = false;
    MonoWorldReply reply;
};

// A fila recebe cópias: nenhum ponteiro IL2CPP atravessa threads.
// Invalidar uma sessão descarta respostas antigas, mesmo se o callback atrasar.
class MonoWorldMailbox {
public:
    static constexpr size_t Limit = 8;
    static constexpr size_t MaxPayload = 4 * ((1024 * 1024 + 2) / 3);
    static constexpr size_t MaxNetPayload = 4 * ((60000 + 2) / 3);

    std::shared_ptr<MonoWorldTicket> submit(MonoWorldRequest request) {
        if (!valid(request)) return {};
        std::lock_guard<std::mutex> guard(mutex_);
        if (jobs_.size() >= Limit) return {};
        auto job = std::make_shared<MonoWorldTicket>();
        job->request = std::move(request);
        job->generation = generation_;
        jobs_.push_back(job);
        return job;
    }

    bool take(std::shared_ptr<MonoWorldTicket>* out) {
        if (!out) return false;
        std::lock_guard<std::mutex> guard(mutex_);
        if (jobs_.empty()) return false;
        auto job = jobs_.front();
        jobs_.pop_front();
        active_ = job;
        *out = job;
        return true;
    }

    bool current(const std::shared_ptr<MonoWorldTicket>& job) const {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!job || job->generation != generation_) return false;
        std::lock_guard<std::mutex> check(job->mutex);
        return !job->done && !job->cancelled;
    }

    void finish(const std::shared_ptr<MonoWorldTicket>& job, MonoWorldReply reply) {
        if (!job) return;
        std::lock_guard<std::mutex> guard(mutex_);
        {
            std::lock_guard<std::mutex> check(job->mutex);
            if (job->generation != generation_ || job->cancelled) {
                job->cancelled = true;
            } else {
                job->reply = std::move(reply);
            }
            job->done = true;
        }
        job->wake.notify_all();
        if (active_ == job) active_.reset();
    }

    bool wait(const std::shared_ptr<MonoWorldTicket>& job,
              std::chrono::milliseconds deadline, MonoWorldReply* result) {
        if (!job || !result || deadline.count() <= 0) return false;
        std::unique_lock<std::mutex> lock(job->mutex);
        if (!job->wake.wait_for(lock, deadline,
            [&] { return job->done || job->cancelled; })) {
            job->cancelled = true;
            return false;
        }
        if (!job->done || job->cancelled) return false;
        *result = job->reply;
        return true;
    }

    bool pending() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return !jobs_.empty();
    }

    void invalidate() {
        std::lock_guard<std::mutex> guard(mutex_);
        ++generation_;
        for (const auto& job : jobs_) cancel(job);
        jobs_.clear();
        if (active_) cancel(active_);
        active_.reset();
    }

private:
    static bool valid(const MonoWorldRequest& request) {
        switch (request.action) {
            case MonoWorldAction::Register:
                return request.modUid.size() == 36 && request.assemblyPath.size() > 4 &&
                       request.assemblyPath.size() <= 2048 &&
                       request.assemblyPath.front() == '/' &&
                       request.assemblyPath.find("..") == std::string::npos &&
                       request.payload.empty();
            case MonoWorldAction::Enter:
                return request.epoch > 0 && request.payload.size() <= MaxPayload;
            case MonoWorldAction::Confirm:
                return request.epoch > 0 && !request.payload.empty() &&
                       request.payload.size() <= MaxPayload;
            case MonoWorldAction::NetReceive:
                return request.epoch > 0 && !request.payload.empty() &&
                       request.payload.size() <= MaxNetPayload;
            case MonoWorldAction::Prepare:
            case MonoWorldAction::NetSend:
            case MonoWorldAction::Leave:
                return request.epoch > 0 && request.payload.empty();
        }
        return false;
    }

    static void cancel(const std::shared_ptr<MonoWorldTicket>& job) {
        {
            std::lock_guard<std::mutex> check(job->mutex);
            job->cancelled = true;
        }
        job->wake.notify_all();
    }

    mutable std::mutex mutex_;
    uint64_t generation_ = 1;
    std::deque<std::shared_ptr<MonoWorldTicket>> jobs_;
    std::shared_ptr<MonoWorldTicket> active_;
};
}
