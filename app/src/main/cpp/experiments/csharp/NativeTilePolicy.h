#pragma once
#include <cstdint>

namespace bl::csharp::tile_policy {
inline constexpr int kSupportedManagedCallbacks = (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<9)|(1<<10)|(1<<11)|(1<<12)|(1<<13)|(1<<14)|(1<<15);
inline bool validCallbackMask(int mask) { return mask >= 0 && (mask & ~kSupportedManagedCallbacks) == 0; }


inline int leftOf2x1(int x, int frameX) {
    if (frameX < 0) return x;
    return x - ((frameX % 36) / 18);
}

inline bool shouldDrop(int shape, bool includeLargeObjectDrops) {
    return shape == 0 || includeLargeObjectDrops;
}

inline bool shouldDrop(int shape, bool large, bool allowDrops) {
    return allowDrops && shouldDrop(shape, large);
}

inline bool blocksCanKillTile(bool allowBreaking) {
    return !allowBreaking;
}

inline int dustBudget(bool fail, int failCount, int normalCount) {
    return fail ? failCount : normalCount;
}

inline bool incomplete2x1(bool left, bool right, bool leftSupport, bool rightSupport) {
    return !left || !right || !leftSupport || !rightSupport;
}

}
