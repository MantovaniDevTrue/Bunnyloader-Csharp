#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace bl::csharp {

#if defined(BUNNY_CSHARP_DUST_PREVIEW) && BUNNY_CSHARP_DUST_PREVIEW == 1
inline constexpr bool managedDustCallbacksConnected() noexcept { return true; }
#else
inline constexpr bool managedDustCallbacksConnected() noexcept { return false; }
#endif

enum ManagedDustHook : std::uint32_t {
    DustOnSpawn = 1u,
    DustUpdate = 2u,
    DustMidUpdate = 4u,
    DustGetAlpha = 8u,
    DustPreDraw = 16u
};

struct ManagedDustState {
    std::int32_t slot;
    std::int32_t type;
    std::int32_t generation;
    std::int32_t active;
    float x;
    float y;
    float velocityX;
    float velocityY;
    float scale;
    float rotation;
    std::int32_t alpha;
    std::int32_t frameX;
    std::int32_t frameY;
    std::int32_t frameWidth;
    std::int32_t frameHeight;
    std::uint32_t color;
};

static_assert(std::is_standard_layout_v<ManagedDustState>);
static_assert(std::is_trivially_copyable_v<ManagedDustState>);
static_assert(sizeof(ManagedDustState) == 64);
static_assert(alignof(ManagedDustState) == 4);
static_assert(offsetof(ManagedDustState, slot) == 0);
static_assert(offsetof(ManagedDustState, generation) == 8);
static_assert(offsetof(ManagedDustState, x) == 16);
static_assert(offsetof(ManagedDustState, scale) == 32);
static_assert(offsetof(ManagedDustState, alpha) == 40);
static_assert(offsetof(ManagedDustState, color) == 60);

inline bool managedDustStateValid(const ManagedDustState& state) noexcept {
    return state.slot >= 0 && state.slot < 6000 &&
           state.type >= 332 && state.type < 32768 &&
           state.generation > 0 && (state.active == 0 || state.active == 1) &&
           std::isfinite(state.x) && std::isfinite(state.y) &&
           std::isfinite(state.velocityX) && std::isfinite(state.velocityY) &&
           std::isfinite(state.scale) && (state.active == 0 || state.scale > 0.f) &&
           std::isfinite(state.rotation) &&
           state.alpha >= 0 && state.alpha <= 255 &&
           state.frameX >= 0 && state.frameY >= 0 &&
           state.frameWidth >= 0 && state.frameHeight >= 0;
}

inline bool managedDustSameInstance(const ManagedDustState& first,
                                   const ManagedDustState& second) noexcept {
    return first.slot == second.slot &&
           first.type == second.type &&
           first.generation == second.generation;
}

inline bool mergeManagedDustDelta(const ManagedDustState& live,
                                  const ManagedDustState& before,
                                  const ManagedDustState& after,
                                  ManagedDustState* output,
                                  bool updateColor = true) noexcept {
    if (!output || !managedDustStateValid(live) ||
        !managedDustStateValid(before) || !managedDustStateValid(after) ||
        !managedDustSameInstance(live, before) ||
        !managedDustSameInstance(before, after))
        return false;
    ManagedDustState merged = live;
    if (after.active == 0) {
        merged.active = 0;
        *output = merged;
        return true;
    }
    if (after.x != before.x) merged.x += after.x - before.x;
    if (after.y != before.y) merged.y += after.y - before.y;
    if (after.velocityX != before.velocityX)
        merged.velocityX += after.velocityX - before.velocityX;
    if (after.velocityY != before.velocityY)
        merged.velocityY += after.velocityY - before.velocityY;
    if (after.scale != before.scale && before.scale > 0.f)
        merged.scale *= after.scale / before.scale;
    if (after.rotation != before.rotation)
        merged.rotation += after.rotation - before.rotation;
    if (after.alpha != before.alpha) {
        const int alpha = merged.alpha + after.alpha - before.alpha;
        merged.alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    }
    if (after.frameX != before.frameX) merged.frameX = after.frameX;
    if (after.frameY != before.frameY) merged.frameY = after.frameY;
    if (after.frameWidth != before.frameWidth) merged.frameWidth = after.frameWidth;
    if (after.frameHeight != before.frameHeight) merged.frameHeight = after.frameHeight;
    if (updateColor && after.color != before.color) merged.color = after.color;
    if (!managedDustStateValid(merged)) return false;
    *output = merged;
    return true;
}


}
