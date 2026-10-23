#pragma once

#include <cmath>

namespace bl::csharp {

struct TileCollisionVector {
    float x = 0.f;
    float y = 0.f;
};

inline bool managedTileCollisionModeSupported(int mode) {
    return mode >= 1 && mode <= 3;
}

inline bool managedTileCollisionDetected(
    TileCollisionVector before, TileCollisionVector after, bool tileCollide) {
    return tileCollide && std::isfinite(before.x) && std::isfinite(before.y) &&
        std::isfinite(after.x) && std::isfinite(after.y) &&
        (before.x != after.x || before.y != after.y);
}

inline bool shouldDeferManagedTileCollisionKill(
    int mode, TileCollisionVector before, TileCollisionVector after, bool tileCollide) {
    return managedTileCollisionModeSupported(mode) &&
        managedTileCollisionDetected(before, after, tileCollide);
}

inline TileCollisionVector resolveManagedTileCollision(
    int mode, TileCollisionVector before, TileCollisionVector after, bool tileCollide) {
    if (!shouldDeferManagedTileCollisionKill(mode, before, after, tileCollide))
        return after;
    if (mode == 2) {
        if (before.x != after.x) after.x = -before.x;
        if (before.y != after.y) after.y = -before.y;
    } else if (mode == 3) {
        if (before.x != after.x) after.x = 0.f;
        if (before.y != after.y) after.y = 0.f;
    }
    return after;
}

}
