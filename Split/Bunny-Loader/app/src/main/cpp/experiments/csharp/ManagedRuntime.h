#pragma once

#include <string>

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
