#pragma once

#include <cstdint>

namespace bl::csharp {

struct ProjectileWriteStatus {
    bool installed = false;
    int automaticCalls = 0;
    int automaticWrites = 0;
    int automaticSpawns = 0;
    int automaticReleases = 0;
    bool claimed = false;
    bool valid = false;
    int slot = -1;
    int type = -1;
    int owner = -1;
    int generation = 0;
    int targetIndex = -1;
    int writes = 0;
    int loopCalls = 0;
    int lastSlot = -1;
    int lastType = -1;
    int lastTargetIndex = -1;
    int lastWrites = 0;
    int lastLoopCalls = 0;
};

bool installProjectileWriteTest();
ProjectileWriteStatus claimProjectile(int requestedSlot = -1);
void releaseProjectile();
ProjectileWriteStatus projectileWriteStatus();

}
