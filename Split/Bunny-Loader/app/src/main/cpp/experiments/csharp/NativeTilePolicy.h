#pragma once
#include <cstdint>

namespace bl::csharp::tile_policy {

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
