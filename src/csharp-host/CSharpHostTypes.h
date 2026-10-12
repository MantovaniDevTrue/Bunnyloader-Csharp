#pragma once

#include <cstdint>

namespace bl::csharp {

struct NpcSample {
    int32_t index;
    int32_t type;
    int32_t life;
    int32_t lifeMax;
    float x;
    float y;
    float vx;
    float vy;
    int32_t flags;
};
static_assert(sizeof(NpcSample) == 36);

struct ProjectileSample {
    int32_t index;
    int32_t type;
    int32_t owner;
    int32_t timeLeft;
    int32_t damage;
    float x;
    float y;
    float vx;
    float vy;
    int32_t flags;
};
static_assert(sizeof(ProjectileSample) == 40);

struct PlayerSample {
    int32_t index;
    int32_t life;
    int32_t lifeMax;
    float x;
    float y;
    float vx;
    float vy;
};
static_assert(sizeof(PlayerSample) == 28);

struct HostProjectileState {
    int32_t slot;
    int32_t type;
    int32_t owner;
    int32_t generation;
    int32_t targetIndex;
    float x;
    float y;
    float vx;
    float vy;
    float rotation;
};
static_assert(sizeof(HostProjectileState) == 40);

struct HostNpcState {
    int32_t slot;
    int32_t type;
    int32_t generation;
    int32_t life;
    int32_t lifeMax;
    int32_t targetPlayer;
    float x;
    float y;
    float vx;
    float vy;
};
static_assert(sizeof(HostNpcState) == 40);

struct HostNpcAttack {
    int32_t count;
    int32_t damage;
    float x;
    float y;
    float vx0;
    float vy0;
    float vx1;
    float vy1;
};
static_assert(sizeof(HostNpcAttack) == 32);

}
