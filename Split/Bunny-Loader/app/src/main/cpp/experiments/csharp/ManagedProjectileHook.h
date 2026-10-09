#pragma once

#include <string>
#include <array>

namespace bl::csharp {

struct SourceBoltHomingPolicy {
    int radius = 480;
    int retargetTicks = 8;
    std::array<int, 129> turnsMilliRadians{};
};

struct SourceBoltHitPolicy {
    int buffId = 24;
    std::array<int, 256> durations{};
};

struct SourceBoltHitStatus {
    bool installed = false;
    bool enabled = false;
    int events = 0;
    int applied = 0;
    int errors = 0;
    int lastNpc = -1;
    int lastDamage = -1;
    int lastDuration = 0;
    int buffId = 0;
};

struct SourceBoltKillPolicy {
    int buffId = 24;
    std::array<int, 256> radii{};
    std::array<int, 256> durations{};
};

struct SourceBoltKillStatus {
    bool installed = false;
    bool enabled = false;
    int killEvents = 0;
    int buffsApplied = 0;
    int errors = 0;
    int lastDamage = -1;
    int lastRadius = 0;
    int lastDuration = 0;
    int lastAffected = 0;
    int buffId = 0;
};

struct ManagedProjectileHookStatus {
    bool installed = false;
    bool enabled = false;
    bool buffered = false;
    bool homing = false;
    int homingScans = 0;
    int homingLocks = 0;
    int homingFrames = 0;
    bool hitInstalled = false;
    bool hitEnabled = false;
    int hitEvents = 0;
    int buffsApplied = 0;
    int buffErrors = 0;
    int lastHitNpc = -1;
    int hitBuffId = 0;
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
bool enableSourceBoltOnHit(const SourceBoltHitPolicy& policy, std::string* error);
bool pauseSourceBoltOnHit();
SourceBoltHitStatus sourceBoltHitStatus();
bool enableSourceBoltOnKill(const SourceBoltKillPolicy& policy, std::string* error);
bool pauseSourceBoltOnKill();
SourceBoltKillStatus sourceBoltKillStatus();
bool pauseManagedSourceBoltHook();
ManagedProjectileHookStatus managedSourceBoltHookStatus();

}
