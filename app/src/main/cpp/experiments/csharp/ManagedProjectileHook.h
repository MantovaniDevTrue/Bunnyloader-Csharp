#pragma once

#include <string>
#include <array>
#include "il2cpp/Api.h"

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

struct AutoCombatEffectStatus {
    bool registered = false;
    bool installed = false;
    bool enabled = false;
    int type = -1;
    int events = 0;
    int buffsApplied = 0;
    int errors = 0;
    int buffId = 0;
};

bool installManagedProjectileTileCollisionHooks(std::string* error);

bool enableAutoProjectileOnHit(const std::string& modId,
                               const std::string& projectileName,
                               const SourceBoltHitPolicy& policy,
                               std::string* error);
bool enableAutoProjectileOnKill(const std::string& modId,
                                const std::string& projectileName,
                                const SourceBoltKillPolicy& policy,
                                std::string* error);
AutoCombatEffectStatus autoProjectileHitStatus(
    const std::string& modId, const std::string& projectileName);
AutoCombatEffectStatus autoProjectileKillStatus(
    const std::string& modId, const std::string& projectileName);

struct ManagedProjectileCurveStatus {
    bool registered = false;
    bool installed = false;
    bool enabled = false;
    int type = -1;
    int samples = 120;
    int spawns = 0;
    int callbacks = 0;
    int velocityWrites = 0;
    int errors = 0;
    int lastSlot = -1;
    int lastTurn = 0;
};

struct SourceSparkAiStatus {
    bool installed = false;
    bool enabled = false;
    int type = -1;
    int samples = 0;
    int spawns = 0;
    int callbacks = 0;
    int velocityWrites = 0;
    int errors = 0;
    int lastSlot = -1;
    int lastTurn = 0;
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

bool prepareManagedSummonRuntime(std::string* error);
// Called before native SetDefaults can overwrite a pooled projectile's slot.
void notifyManagedSourceProjectileDefaults(Il2CppObject* projectile);
void resetManagedSourceProjectileSlots();
bool queueManagedSourceProjectileCombat(int eventKind, Il2CppObject* projectile,
                                        int npcIndex, int damage);
bool installManagedSourceEntityHitHook();
bool installManagedSourceEntityKillHook();
bool installManagedEntityProjectileHook();

bool enableManagedProjectileCurve(const std::string& modId,
                                  const std::string& projectileName,
                                  const std::array<int, 120>& turns,
                                  std::string* error, bool allowInMenu = false);
bool pauseManagedProjectileCurve(const std::string& modId, const std::string& projectileName);
ManagedProjectileCurveStatus managedProjectileCurveStatus(
    const std::string& modId, const std::string& projectileName);
bool enableSourceSparkAi(const std::array<int, 120>& turnsMilliRadians, std::string* error);
bool pauseSourceSparkAi();
SourceSparkAiStatus sourceSparkAiStatus();
bool enableManagedSourceBoltHook(std::string* error);
bool enableBufferedSourceBoltHook(const std::array<int, 120>& turnsMilliRadians,
                                  std::string* error);
bool enableHomingSourceBoltHook(const SourceBoltHomingPolicy& policy,
                                 const std::array<int, 120>& fallbackTurns,
                                 std::string* error);
bool enableSourceBoltOnHit(const SourceBoltHitPolicy& policy, std::string* error);
void processManagedNpcBuffEffect(Il2CppObject* npc);
bool pauseSourceBoltOnHit();
SourceBoltHitStatus sourceBoltHitStatus();
bool enableSourceBoltOnKill(const SourceBoltKillPolicy& policy, std::string* error);
bool pauseSourceBoltOnKill();
SourceBoltKillStatus sourceBoltKillStatus();
bool pauseManagedSourceBoltHook();
ManagedProjectileHookStatus managedSourceBoltHookStatus();

}
