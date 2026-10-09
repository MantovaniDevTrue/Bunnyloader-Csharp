#pragma once

#include <string>
#include <array>

namespace bl::csharp {

struct SourceBoltHomingPolicy {
    int radius = 480;
    int retargetTicks = 8;
    std::array<int, 129> turnsMilliRadians{};
};

struct ManagedProjectileHookStatus {
    bool installed = false;
    bool enabled = false;
    bool buffered = false;
    bool homing = false;
    int homingScans = 0;
    int homingLocks = 0;
    int homingFrames = 0;
    int lastTarget = -1;
    int bufferSamples = 0;
    int type = -1;
    int callbacks = 0;
    int velocityWrites = 0;
    int spawns = 0;
    int errors = 0;
    int lastSlot = -1;
    int lastState = 0;
};

bool enableManagedSourceBoltHook(std::string* error);
bool enableBufferedSourceBoltHook(const std::array<int, 120>& turnsMilliRadians,
                                  std::string* error);
bool enableHomingSourceBoltHook(const SourceBoltHomingPolicy& policy,
                                 const std::array<int, 120>& fallbackTurns,
                                 std::string* error);
bool pauseManagedSourceBoltHook();
ManagedProjectileHookStatus managedSourceBoltHookStatus();

}
