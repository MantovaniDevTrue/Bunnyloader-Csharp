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
ManagedRuntimeStatus probeManagedCallbacks(const std::string& assemblyPath, int count);
bool invokeManagedProjectileAI(int tick, int state, int* nextState);

}
