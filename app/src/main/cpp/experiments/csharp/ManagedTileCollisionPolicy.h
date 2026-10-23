#pragma once

#include <cmath>

namespace bl::csharp {

struct TileCollisionVector {
    float x = 0.f;
    float y = 0.f;
};

inline bool managedTileCollisionBudgetAllows(int survived,int limit) {
    return limit==0 || (limit>0 && limit<=64 && survived>=0 && survived<limit);
}

inline bool recordManagedTileCollisionBudget(int* survived,int limit) {
    if(!survived || !managedTileCollisionBudgetAllows(*survived,limit))return false;
    if(limit>0)++*survived;
    return true;
}

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
    int mode, TileCollisionVector before, TileCollisionVector after,
    bool tileCollide, float minimumSpeed = 0.f) {
    if (!managedTileCollisionModeSupported(mode) ||
        !managedTileCollisionDetected(before, after, tileCollide))
        return false;
    if (mode != 2 || minimumSpeed <= 0.f) return true;
    if (!std::isfinite(minimumSpeed) || minimumSpeed > 64.f) return false;
    const float magnitudeSquared = before.x * before.x + before.y * before.y;
    return std::isfinite(magnitudeSquared) &&
        magnitudeSquared >= minimumSpeed * minimumSpeed;
}

inline TileCollisionVector resolveManagedTileCollision(
    int mode, TileCollisionVector before, TileCollisionVector after,
    bool tileCollide, float restitution = 1.f, float minimumSpeed = 0.f) {
    if (!shouldDeferManagedTileCollisionKill(mode, before, after, tileCollide, minimumSpeed))
        return after;
    if (mode == 2) {
        if (!std::isfinite(restitution) || restitution < 0.f || restitution > 2.f)
            return after;
        const TileCollisionVector next{
            before.x != after.x ? -before.x * restitution : after.x,
            before.y != after.y ? -before.y * restitution : after.y
        };
        if (!std::isfinite(next.x) || !std::isfinite(next.y)) return after;
        return next;
    }
    if (mode == 3) {
        if (before.x != after.x) after.x = 0.f;
        if (before.y != after.y) after.y = 0.f;
    }
    return after;
}

}
