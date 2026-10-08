#include "experiments/csharp/ManagedRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const auto fresh = bl::csharp::managedRuntimeStatus();
    if (fresh.attempted || fresh.phase != "idle") return 2;

    const auto missing = bl::csharp::probeManagedRuntime("/invalid/managed/probe.dll");
    if (!missing.attempted || missing.phase != "invalid-assembly" ||
        missing.libraryLoaded || missing.methodExecuted) return 3;

    const auto inspected = bl::csharp::inspectManagedRuntime();
    if (!inspected.libraryLoaded || !inspected.symbolsReady ||
        inspected.domainInitialized || inspected.methodExecuted ||
        inspected.phase != "symbols-ready") return 7;

    const auto first = bl::csharp::probeManagedRuntime(argv[1]);
    if (!first.libraryLoaded || !first.symbolsReady ||
        !first.domainInitialized || !first.methodExecuted ||
        first.phase != "verified" || first.result != 42) {
        std::fprintf(stderr, "phase=%s error=%s\n", first.phase.c_str(), first.error.c_str());
        return 4;
    }
    const auto second = bl::csharp::probeManagedRuntime(argv[1]);
    if (!second.methodExecuted || second.result != 42) return 5;
    std::puts("BUNNY_MANAGED_BRIDGE_MOCK_PASS=YES");
    return 0;
}
