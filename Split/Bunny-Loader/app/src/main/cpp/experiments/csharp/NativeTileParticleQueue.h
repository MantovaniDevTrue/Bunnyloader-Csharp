#pragma once
#include "experiments/csharp/NativeTileParticleWire.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace bl::csharp::tile_particle {
class ParticleQueue {
public:
    bool stage(uint64_t frame,const Batch& input) {
        if(frame==0||input.count==0||input.count>kMaxCellParticles)return false;
        for(size_t i=0;i<input.count;++i)if(!valid(input.items[i]))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(frame<frame_)return false;
        if(frame!=frame_){frame_=frame;count_=0;}
        if(input.count>kMaxFrameParticles-count_)return false;
        for(size_t i=0;i<input.count;++i)items_[count_+i]=input.items[i];
        count_+=input.count;
        pendingFrame_.store(frame,std::memory_order_release);
        pending_.store(true,std::memory_order_release);
        return true;
    }
    bool pendingFor(uint64_t frame) const {
        return pending_.load(std::memory_order_acquire)&&
               pendingFrame_.load(std::memory_order_acquire)==frame;
    }
    bool take(uint64_t frame,std::array<Particle,kMaxFrameParticles>* out,size_t* count) {
        if(!out||!count||!pendingFor(frame))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(frame_!=frame||count_==0)return false;
        *count=count_;
        for(size_t i=0;i<count_;++i)(*out)[i]=items_[i];
        count_=0;
        pending_.store(false,std::memory_order_release);
        return true;
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_=0;count_=0;
        pending_.store(false,std::memory_order_release);
        pendingFrame_.store(0,std::memory_order_release);
    }
private:
    mutable std::mutex mutex_;
    std::atomic<bool> pending_{false};
    std::atomic<uint64_t> pendingFrame_{0};
    std::array<Particle,kMaxFrameParticles> items_{};
    size_t count_=0;
    uint64_t frame_=0;
};
}
