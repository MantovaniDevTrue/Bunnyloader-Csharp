#pragma once
#include "experiments/csharp/NativeTileSpriteWire.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace bl::csharp::tile_sprite {

inline constexpr size_t kMaxStagedSprites = 32;

inline constexpr bool eligibleDrawPass(bool solidLayer,bool intoRenderTargets,bool gameMenu) {
    return !solidLayer && !intoRenderTargets && !gameMenu;
}

class SpritePassQueue {
public:
    bool stage(uint64_t frame,const Batch& batch) {
        if(frame==0 || batch.count==0 || batch.count>kMaxSprites) return false;
        for(size_t index=0;index<batch.count;++index)
            if(!valid(batch.sprites[index]))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(frame<frame_)return false;
        if(frame_!=frame){frame_=frame;count_=0;}
        if(batch.count>kMaxStagedSprites-count_)return false;
        for(size_t index=0;index<batch.count;++index)
            sprites_[count_+index]=batch.sprites[index];
        count_+=batch.count;
        pendingFrame_.store(frame_,std::memory_order_release);
        pending_.store(count_!=0,std::memory_order_release);
        return true;
    }

    bool take(uint64_t frame,std::array<Sprite,kMaxStagedSprites>* output,size_t* length) {
        if(!output||!length||!pending_.load(std::memory_order_acquire))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(frame_!=frame||count_==0)return false;
        *length=count_;
        for(size_t index=0;index<count_;++index)
            (*output)[index]=sprites_[index];
        count_=0;
        pending_.store(false,std::memory_order_release);
        return true;
    }

    bool pending() const {return pending_.load(std::memory_order_acquire);}
    bool pendingFor(uint64_t frame) const {
        return pending() && pendingFrame_.load(std::memory_order_acquire)==frame;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_=0;
        count_=0;
        pending_.store(false,std::memory_order_release);
        pendingFrame_.store(0,std::memory_order_release);
    }

private:
    std::mutex mutex_;
    std::atomic<bool> pending_{false};
    std::atomic<uint64_t> pendingFrame_{0};
    std::array<Sprite,kMaxStagedSprites> sprites_{};
    size_t count_=0;
    uint64_t frame_=0;
};

}
