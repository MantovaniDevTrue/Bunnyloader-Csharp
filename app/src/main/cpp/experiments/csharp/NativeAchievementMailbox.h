#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::achievement {

struct Event {
    int kind = 0;
    int type = 0;
    int stack = 1;
    bool local = true;
};

struct Delivery {
    std::string mod;
    std::string assembly;
    uint64_t epoch = 0;
    Event event;
    int attempts = 0;
};

struct Candidate {
    std::string mod;
    std::string assembly;
    uint64_t epoch = 0;
};

class Mailbox {
    struct Owner {
        std::string mod;
        std::string assembly;
        uint64_t epoch = 0;
        bool ready = false;
        bool empty = false;
    };

    mutable std::mutex mutex_;
    std::vector<Owner> owners_;
    std::deque<Delivery> queue_;
    uint64_t nextEpoch_ = 1;
    uint64_t dropped_ = 0;

    const Owner* find(const Delivery& message) const {
        for (const auto& owner : owners_)
            if (owner.mod == message.mod && owner.assembly == message.assembly &&
                owner.epoch == message.epoch && owner.ready && !owner.empty)
                return &owner;
        return nullptr;
    }

public:
    static constexpr size_t kMaxOwners = 16;
    static constexpr size_t kMaxQueue = 512;

    bool bind(const std::string& mod, const std::string& assembly) {
        if (mod.empty() || mod.size() > 96 || assembly.empty() ||
            assembly.size() > 4096) return false;
        std::lock_guard guard(mutex_);
        for (auto& owner : owners_) {
            if (owner.mod != mod) continue;
            if (owner.assembly == assembly) return true;
            owner.assembly = assembly;
            owner.epoch = nextEpoch_++;
            owner.ready = false;
            owner.empty = false;
            std::erase_if(queue_, [&](const Delivery& delivery) {
                return delivery.mod == mod;
            });
            return true;
        }
        if (owners_.size() >= kMaxOwners) return false;
        owners_.push_back({mod, assembly, nextEpoch_++, false, false});
        return true;
    }

    void retire(const std::string& mod) {
        std::lock_guard guard(mutex_);
        std::erase_if(owners_, [&](const Owner& owner) { return owner.mod == mod; });
        std::erase_if(queue_, [&](const Delivery& delivery) { return delivery.mod == mod; });
    }

    bool activate(const Candidate& candidate, int count) {
        if (count < 0 || count > 128) return false;
        std::lock_guard guard(mutex_);
        for (auto& owner : owners_) {
            if (owner.mod != candidate.mod || owner.assembly != candidate.assembly ||
                owner.epoch != candidate.epoch) continue;
            owner.ready = count != 0;
            owner.empty = count == 0;
            return true;
        }
        return false;
    }

    std::vector<Candidate> pending(size_t limit = 2) const {
        std::lock_guard guard(mutex_);
        std::vector<Candidate> result;
        for (const auto& owner : owners_) {
            if (owner.ready || owner.empty) continue;
            result.push_back({owner.mod, owner.assembly, owner.epoch});
            if (result.size() >= limit) break;
        }
        return result;
    }

    bool anyReady() const {
        std::lock_guard guard(mutex_);
        return std::any_of(owners_.begin(), owners_.end(),
                           [](const Owner& owner) { return owner.ready; });
    }

    bool publish(const Event& event) {
        if (event.kind < 0 || event.kind > 3 || event.type < -65 ||
            event.type > 32767 || event.stack <= 0 || event.stack > 999999)
            return false;
        std::lock_guard guard(mutex_);
        bool accepted = false;
        for (const auto& owner : owners_) {
            if (!owner.ready) continue;
            if (queue_.size() >= kMaxQueue) {
                ++dropped_;
                continue;
            }
            queue_.push_back({owner.mod, owner.assembly, owner.epoch, event, 0});
            accepted = true;
        }
        return accepted;
    }

    std::vector<Delivery> drain(size_t maximum = 8) {
        std::lock_guard guard(mutex_);
        std::vector<Delivery> selected;
        while (!queue_.empty() && selected.size() < maximum) {
            Delivery event = std::move(queue_.front());
            queue_.pop_front();
            if (find(event)) selected.push_back(std::move(event));
        }
        return selected;
    }

    bool retry(Delivery event) {
        std::lock_guard guard(mutex_);
        if (!find(event) || event.attempts >= 2 || queue_.size() >= kMaxQueue)
            return false;
        ++event.attempts;
        queue_.push_back(std::move(event));
        return true;
    }

    void newWorld() {
        std::lock_guard guard(mutex_);
        queue_.clear();
    }

    uint64_t dropped() const {
        std::lock_guard guard(mutex_);
        return dropped_;
    }

    size_t size() const {
        std::lock_guard guard(mutex_);
        return queue_.size();
    }
};

}
