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
    std::string phase = "idle";
    std::string error;
};

ManagedRuntimeStatus managedRuntimeStatus();
ManagedRuntimeStatus probeManagedRuntime(const std::string& assemblyPath);

}
