#pragma once
#include "experiments/csharp/ManagedHitContextStats.h"
#include "experiments/csharp/ManagedVisualQueue.h"
#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedNpcBuffEffects.h"

#include <string>
#include <cstdint>
#include <vector>

namespace bl::graphics { struct ScreenSceneRaw; }

namespace bl::csharp {
bool managedRecipeRuntimeReady();
bool invokeManagedTile(const std::string& mod,const std::string& assembly,
    const std::string& owner,int callback,int x,int y,std::string* output,int budgetMicroseconds=1800);
bool invokeManagedTileKill(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,bool fail,bool effectOnly,bool noItem,
    std::string* output,int budgetMicroseconds=1100);
bool queueManagedTileMultiKill(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,int frameX,int frameY);
bool invokeManagedTileFrame(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,bool reset,bool noBreak,
    std::string* output,int budgetMicroseconds=650);
bool invokeManagedTileSmart(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,int lx,int ly,int hx,int hy,
    std::string* output,int budgetMicroseconds=450);
bool invokeManagedTileDraw(const std::string& mod,const std::string& assembly,
    const std::string& owner,const std::string& snapshot,
    std::string* output,int budgetMicroseconds=1200);
bool queueManagedTileEvent(const std::string& mod,const std::string& assembly,
    const std::string& owner,int callback,int x,int y);
bool queueManagedTileNearby(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,bool closer);
bool queueManagedTileWarmup(const std::string& mod,const std::string& assembly,const std::string& owner);
int managedTileWarmupState(const std::string& mod,const std::string& assembly,
    const std::string& owner,std::string* error);
// Shares the bounded worker transport, with its own dispatcher and prewarmed content instances.
bool queueManagedShootWarmup(const std::string& mod,const std::string& assembly,const std::string& owner);
int managedShootWarmupState(const std::string& mod,const std::string& assembly,const std::string& owner,std::string* error);
bool invokeManagedShoot(const std::string& mod,const std::string& assembly,const std::string& owner,
                        int kind,const std::string& snapshot,std::string* output);

// Bounded request to the existing pinned Mono worker. No live IL2CPP objects cross threads.
bool invokeManagedRecipe(const std::string& mod,const std::string& assembly,
                         const std::string& owner,int index,int kind,const std::string& snapshot,
                         std::string* output,int budgetMicroseconds=3000);
struct ManagedScreenLightResult {
    float sunX = 0, sunY = 0, clock = 0, strength = 0;
    float shadowThreshold = 0;
    float baseSunX = 0, baseSunY = 0, baseClock = 0;
    bool enabled = false;
};
bool registerManagedScreenSceneMod(const std::string& modId,
                                   const std::string& assemblyPath,
                                   const std::string& nameSpace,
                                   const std::string& className,
                                   const std::string& method);
bool queueManagedScreenScene(const std::string& modId,
                             const graphics::ScreenSceneRaw& raw,
                             float baseSunX, float baseSunY, float baseClock);
bool readManagedScreenScene(const std::string& modId,
                            ManagedScreenLightResult* result);
void pauseManagedScreenSceneCallbacks();


struct ManagedHurtResult {
    int damage = 0;
    int hitDirection = 0;
    int flags = 0;
    bool cancelled = false;
    bool dodged = false;
};

struct ManagedHurtSchedulerStatus {
    uint64_t submitted = 0;
    uint64_t completed = 0;
    uint64_t timedOut = 0;
    uint64_t cancelledBeforeRun = 0;
    uint64_t lateResults = 0;
    uint64_t waitTotalUs = 0;
    uint64_t waitMaxUs = 0;
    int pending = 0;
    int timeoutMs = 5;
};

ManagedHurtSchedulerStatus managedHurtSchedulerStatus();

struct ManagedEntityVelocity
{
    float x = 0;
    float y = 0;
};

bool registerManagedEntitySourceMod(const std::string& modId,
                                    const std::string& assemblyPath,
                                    const std::vector<std::pair<int, std::string>>& npcTypes,
                                    const std::vector<std::pair<int, std::string>>& projectileTypes);
bool hasManagedEntityNpcType(int type);
bool hasManagedEntityProjectileType(int type);
bool managedEntitySourceModsRegistered();
void pauseManagedEntitySourceCallbacks();
bool queueManagedEntityCombatEvent(int eventKind, int type, int slot, int generation,
                                   int owner, uintptr_t identity, int damage, int targetIndex);

bool tickManagedEntityNpc(int type, int slot, int generation,
                          uintptr_t identity, int life, int lifeMax,
                          float x, float y, float vx, float vy,
                          float playerX, float playerY, ManagedEntityVelocity* result);
bool tickManagedEntityProjectile(int type, int slot, int generation, int owner,
                                 uintptr_t identity, float x, float y,
                                 float vx, float vy, float playerX, float playerY,
                                 ManagedEntityVelocity* result, bool immediateFirstTick = false,
                                 int targetNpc = -1, float targetX = 0.f,
                                 float targetY = 0.f);

bool registerManagedPlayerHurtMod(const std::string& modId,
                                  const std::string& assemblyPath);
void pauseManagedPlayerHurtMods();
using ManagedHurtDiagnosticLogFn = void (*)(const char*);
void setManagedHurtDiagnosticLogger(ManagedHurtDiagnosticLogFn logger);
bool modifyManagedPlayerHurt(const std::string& modId, int damage,
                             int hitDirection, int flags, int life, int lifeMax,
                             int equipmentMask, int npcIndex, int projectileIndex,
                             int npcType, int npcDamage, int npcLife,
                             int projectileType, int projectileDamage, int projectileOwner,
                             float attackerX, float attackerY,
                             float attackerVelocityX, float attackerVelocityY,
                             float attackerAngle, float attackerDistance,
                             ManagedHurtResult* result);
bool notifyManagedPlayerHurt(const std::string& modId, int sourceDamage,
                             int actualDamage, int hitDirection, int flags,
                             int lifeRemaining, int equipmentMask,
                             int npcIndex, int projectileIndex,
                             int npcType, int npcDamage, int npcLife,
                             int projectileType, int projectileDamage, int projectileOwner,
                             float attackerX, float attackerY,
                             float attackerVelocityX, float attackerVelocityY,
                             float attackerAngle, float attackerDistance);

struct ManagedRuntimeStatus {
    bool attempted = false;
    bool libraryLoaded = false;
    bool symbolsReady = false;
    bool domainInitialized = false;
    bool methodExecuted = false;
    int result = 0;
    int callbacksExecuted = 0;
    std::string phase = "idle";
    std::string error;
};

bool invokeInteractiveCSharp(const std::string& assemblyPath, std::string* output, std::string* error);
void setManagedCheckpointPath(const std::string& path);
std::string managedHurtHookSnapshot();
ManagedRuntimeStatus managedRuntimeStatus();
ManagedRuntimeStatus inspectManagedRuntime();
ManagedRuntimeStatus probeManagedRuntime(const std::string& assemblyPath);
ManagedRuntimeStatus probeManagedRuntimePinnedThread(
    const std::string& assemblyPath, const std::string& stage = "PINNED_MONO");
long managedRuntimePinnedThreadTid();

struct ManagedMemoryStatus {
    bool available = false;
    bool monoReady = false;
    int64_t rssKb = 0;
    int64_t peakRssKb = 0;
    int64_t virtualKb = 0;
    int64_t dataKb = 0;
    int64_t swapKb = 0;
    int threads = 0;
    int64_t baselineRssKb = 0;
    int64_t rssDeltaKb = 0;
    int cachedCallbackEntries = 0;
    int cachedAssemblyPaths = 0;
    bool monoGcStatsAvailable = false;
    int64_t monoGcHeapBytes = 0;
    int64_t monoGcUsedBytes = 0;
    uint64_t liveMethodCacheHits = 0;
    uint64_t liveMethodCacheMisses = 0;
    uint64_t liveAssemblyOpenAttempts = 0;
    uint64_t liveAssemblyOpenSuccesses = 0;
    int distinctLiveOpenedAssemblyPaths = 0;
    int boundProjectileTypes = 0;
    int activeProjectileTypes = 0;
    long workerTid = 0;
};

ManagedMemoryStatus managedMemoryStatus(bool resetBaseline = false);
ManagedRuntimeStatus probeManagedCallbacks(const std::string& assemblyPath, int count);
bool invokeManagedProjectileAI(int tick, int state, int* nextState);

struct LiveSourceStatus {
    bool loaded = false;
    bool ready = false;
    int callbacks = 0;
    int failures = 0;
    int lastTick = -1;
    int lastState = 0;
    int lastResult = 0;
    int npcCallbacks = 0;
    int npcFailures = 0;
    int npcLastLife = 0;
    int npcLastDeltaX = 0;
    int npcLastVelocityTenths = 0;
    int npcThreadAttaches = 0;
    int npcThreadDetaches = 0;
    int npcThreadAlreadyAttached = 0;
    std::string modId;
    std::string phase = "idle";
    std::string error;
};

LiveSourceStatus liveSourceStatus(const std::string& modId);
bool invokeLiveSourceNpcAi(const std::string& modId,
                           const std::string& assemblyPath,
                           int playerDeltaX, int npcLife,
                           int* velocityTenths);

LiveSourceStatus invokeLiveSourceTick(const std::string& modId,
                                     const std::string& assemblyPath,
                                     int tick, int state);

struct ManagedLiveHitStatus {
    ManagedNpcBuffEffectStatus effects;
    ManagedHitContextStats hitContext;
    bool enabled = false;
    bool automatic = false;
    bool primaryEnabled = false;
    int bindingCapacity = 32;
    int perBindingQueueCapacity = 64;
    int primaryQueueDrops = 0;
    int projectileType = -1;
    int queued = 0;
    int processed = 0;
    int hitProcessed = 0;
    int killProcessed = 0;
    int dropped = 0;
    int lockDrops = 0;
    int queueDepth = 0;
    int maxQueueDepth = 0;
    int queueCapacity = 64;
    int eventLimit = 25000;
    bool inFlight = false;
    int failures = 0;
    int lastDamage = 0;
    int lastResult = 0;
    std::string lastEvent;
    std::string modId;
    std::string phase = "idle";
    std::string error;
};

bool enableManagedLiveHit(const std::string& modId, const std::string& assemblyPath,
                          int projectileType, std::string* error,
                          bool automatic = false);
ManagedLiveHitStatus managedLiveHitStatus();
ManagedLiveHitStatus pauseManagedLiveHit();

struct ManagedLiveProjectileStatus {
    ManagedHitContextStats hitContext;
    std::string modId;
    std::string projectile;
    std::string callbackClass;
    int projectileType = -1;
    bool enabled = false;
    int queued = 0;
    int processed = 0;
    int hits = 0;
    int kills = 0;
    int failures = 0;
    int dropped = 0;
    int lastResult = 0;
    std::string error;
};

bool enableManagedLiveMod(const std::string& modId,
                          const std::string& assemblyPath,
                          int projectileType,
                          const std::string& projectile,
                          std::string* error);
bool pauseManagedLiveMod(const std::string& modId, std::string* error);
bool resumeManagedLiveMod(const std::string& modId, std::string* error);
bool enableManagedLiveProjectile(const std::string& modId,
                                const std::string& assemblyPath,
                                int projectileType,
                                const std::string& projectile,
                                std::string* error);
bool isManagedLiveProjectileBound(int projectileType);
std::vector<ManagedLiveProjectileStatus> managedLiveProjectileStatuses();
void queueManagedLiveHit(int projectileType, int damage, int npcIndex = -1,
                         ManagedNpcSnapshot npc = {},
                         ManagedPlayerSnapshot player = {});
void queueManagedLiveKill(int projectileType, int damage);
void setManagedNpcBuffGameHookReady(bool ready);
bool takeManagedNpcBuffForNpc(int npcIndex, int npcType,
                              float npcX, float npcY,
                              ManagedNpcBuffCommand* result);
void noteManagedNpcBuffApplied(bool success);
void noteManagedNpcImpulseApplied(bool success);
bool hasPendingManagedNpcBuffEffect();
ManagedNpcBuffEffectStatus managedNpcBuffEffectStatus();

struct ManagedLivePerfProjectileStatus {
    std::string modId;
    std::string callbackClass;
    int projectileType = -1;
    bool enabled = false;
    int callbacksSampled = 0;
    int hitSamples = 0;
    int killSamples = 0;
    int failures = 0;
    int queueWaitSamples = 0;
    double queueWaitAvgUs = 0;
    int queueWaitMaxUs = 0;
    double callbackAvgUs = 0;
    int callbackMaxUs = 0;
    int callbackP90Us = 0;
    double callbackCpuAvgUs = 0;
    int callbackCpuMaxUs = 0;
};

struct ManagedLivePerfStatus {
    bool started = false;
    bool recording = false;
    std::string phase = "idle";
    int targetMs = 15000;
    int elapsedMs = 0;
    int updateSamples = 0;
    double updateHz = 0;
    double updateP90Ms = 0;
    double updateP99Ms = 0;
    int updatesOver25Ms = 0;
    int updatesOver33Ms = 0;
    int callbacksSampled = 0;
    double queueWaitAvgUs = 0;
    int queueWaitMaxUs = 0;
    int queueWaitSamples = 0;
    int queueWaitP90Us = 0;
    int queueWaitP99Us = 0;
    double callbackAvgUs = 0;
    int callbackMaxUs = 0;
    int callbackP90Us = 0;
    int callbackP99Us = 0;
    double callbackCpuAvgUs = 0;
    int callbackCpuMaxUs = 0;
    int callbackCpuP90Us = 0;
    int eventsAccepted = 0;
    int eventsProcessed = 0;
    int eventsFailed = 0;
    int eventsQueueDropped = 0;
    int maxQueueDepth = 0;
    int projectileCurveUpdates = 0;
    int projectileVelocityWrites = 0;
    int lockDrops = 0;
    std::vector<ManagedLivePerfProjectileStatus> projectiles;
};

bool startManagedLivePerf(std::string* error);
ManagedLivePerfStatus managedLivePerfStatus();
ManagedLivePerfStatus stopManagedLivePerf();
void noteManagedLivePerfUpdate();
void noteManagedLivePerfProjectileCurveUpdate();
void noteManagedLivePerfProjectileVelocityWrite();

struct VmCompareStatus {
    bool started = false;
    bool running = false;
    bool finished = false;
    bool passed = false;
    int iterations = 0;
    int kind = 0;
    int seed = 0;
    int jsResult = 0;
    int monoResult = 0;
    int64_t jsWallUs = 0;
    int64_t jsThreadCpuUs = 0;
    int64_t monoWallUs = 0;
    int64_t monoThreadCpuUs = 0;
    int rssBeforeJsKb = 0;
    int rssAfterJsKb = 0;
    int rssBeforeMonoKb = 0;
    int rssAfterMonoKb = 0;
    std::string phase = "idle";
    std::string error;
};

bool beginVmCompare(const VmCompareStatus& jsPart, std::string* error);
VmCompareStatus vmCompareStatus();

struct ManagedLiveBenchmarkStatus {
    bool started = false;
    bool running = false;
    bool finished = false;
    bool passed = false;
    int target = 25000;
    int accepted = 0;
    int processed = 0;
    int hits = 0;
    int kills = 0;
    int dropped = 0;
    int maxQueueDepth = 0;
    int elapsedMs = 0;
    std::string phase = "idle";
    std::string error;
};

bool startManagedLiveBenchmark(std::string* error);
ManagedLiveBenchmarkStatus managedLiveBenchmarkStatus();
ManagedLiveBenchmarkStatus stopManagedLiveBenchmark();

struct ManagedSourceCompileResult {
    int result = 0;
    int executedTypes = 0;
    int64_t monoProbeMs = 0;
    int64_t roslynInvokeMs = 0;
    int64_t verifyInvokeMs = 0;
    std::string phase = "idle";
    std::string error;
};

ManagedSourceCompileResult compileManagedSource(
    const std::string& probeAssembly,
    const std::string& compilerAssembly,
    const std::string& modRoot,
    const std::string& outputAssembly,
    const std::string& diagnosticsPath,
    bool precompileOnly = false,
    bool verifyOnly = false);

}

namespace bl::csharp {
size_t pollManagedVisualCommands(const std::string& assemblyPath, size_t budget = 32);
bool takeManagedVisualCommand(ManagedVisualCommand& command);
bool postManagedVisualCommand(ManagedVisualCommand command);
size_t advanceManagedVisualState(uint64_t tick, size_t budget = 32);
size_t activeManagedVisualCount();
std::vector<ManagedVisualCommand> snapshotManagedVisualCommands(int playerIndex, size_t limit = 4);
void visitManagedVisualCommands(int playerIndex, void (*visitor)(const ManagedVisualCommand&,void*), void* context, size_t limit=8);
void clearManagedVisualCommands();
}
