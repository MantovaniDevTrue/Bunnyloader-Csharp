#pragma once

#include <string>
#include <cstdint>

namespace bl::csharp {

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

void setManagedCheckpointPath(const std::string& path);
ManagedRuntimeStatus managedRuntimeStatus();
ManagedRuntimeStatus inspectManagedRuntime();
ManagedRuntimeStatus probeManagedRuntime(const std::string& assemblyPath);
ManagedRuntimeStatus probeManagedRuntimePinnedThread(
    const std::string& assemblyPath, const std::string& stage = "PINNED_MONO");
long managedRuntimePinnedThreadTid();
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
    bool enabled = false;
    bool automatic = false;
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
void queueManagedLiveHit(int projectileType, int damage);
void queueManagedLiveKill(int projectileType, int damage);

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
    std::string phase = "idle";
    std::string error;
};

ManagedSourceCompileResult compileManagedSource(
    const std::string& probeAssembly,
    const std::string& compilerAssembly,
    const std::string& modRoot,
    const std::string& outputAssembly,
    const std::string& diagnosticsPath);

}
