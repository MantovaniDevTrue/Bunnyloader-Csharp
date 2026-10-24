#pragma once
#include <atomic>
#include <mutex>

namespace bl::csharp::worldsave {

// Mobile 301720 SaveAndQuitCallBack sets Main.gameMenu before SaveWorld.
// The UI observer must not treat that early menu change as world disposal.
// Terraria invokes one quit callback per world; this scope spans the original
// callback AND managed unload. The short transition lock closes the race with
// an observer that already passed its initial check. No lock is held while
// game or mod callbacks execute, so the game thread can service native RPCs.
class WorldQuitSaveFence final {
public:
    bool active() const { return saving_.load(std::memory_order_acquire); }

    class Scope final {
    public:
        Scope(WorldQuitSaveFence& fence, std::mutex& transition)
            : fence_(fence), transition_(transition) {
            std::lock_guard lock(transition_);
            fence_.saving_.store(true, std::memory_order_release);
        }
        ~Scope() {
            std::lock_guard lock(transition_);
            fence_.saving_.store(false, std::memory_order_release);
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        WorldQuitSaveFence& fence_;
        std::mutex& transition_;
    };
private:
    std::atomic<bool> saving_{false};
};
}
