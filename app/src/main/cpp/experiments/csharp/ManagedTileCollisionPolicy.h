#pragma once

#include <cmath>

namespace bl::csharp {

struct TileCollisionVector {
    float x = 0.f;
    float y = 0.f;
};

inline bool managedTileCollisionDetected(
    TileCollisionVector before, TileCollisionVector after, bool tileCollide) {
    return tileCollide && std::isfinite(before.x) && std::isfinite(before.y) &&
        std::isfinite(after.x) && std::isfinite(after.y) &&
        (before.x != after.x || before.y != after.y);
}

inline bool shouldDeferManagedTileCollisionKill(
    int mode, TileCollisionVector before, TileCollisionVector after, bool tileCollide) {
    return mode == 1 && managedTileCollisionDetected(before, after, tileCollide);
}

}
