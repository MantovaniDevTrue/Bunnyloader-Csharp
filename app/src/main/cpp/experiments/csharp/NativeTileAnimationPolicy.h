#pragma once
#include <cstdint>

namespace bl::csharp::tile_animation {
struct Policy {
    int frames = 1;
    int ticks = 0;
};
inline bool valid(int height, const Policy& value) {
    return height >= 0 && height <= 4096 &&
           value.frames >= 1 && value.frames <= 64 &&
           value.ticks >= 0 && value.ticks <= 600 &&
           (value.frames == 1 ? value.ticks == 0 : height > 0 && value.ticks > 0);
}
inline void advance(int32_t& frame, int32_t& counter, const Policy& value) {
    if (value.frames <= 1 || value.ticks <= 0) return;
    if (frame < 0 || frame >= value.frames) frame = 0;
    if (counter < 0 || counter >= value.ticks) counter = 0;
    if (++counter >= value.ticks) {
        counter = 0;
        frame = (frame + 1) % value.frames;
    }
}
}
