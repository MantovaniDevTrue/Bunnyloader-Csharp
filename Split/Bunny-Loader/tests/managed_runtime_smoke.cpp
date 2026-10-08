#include "experiments/csharp/ManagedRuntime.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const std::string checkpoint = std::string(argv[1]) + ".checkpoint";
    std::remove(checkpoint.c_str());
    bl::csharp::setManagedCheckpointPath(checkpoint);
    const auto saved = [&] {
        std::ifstream file(checkpoint);
        return std::string(std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>());
    };
    const auto fresh = bl::csharp::managedRuntimeStatus();
    if (fresh.attempted || fresh.phase != "idle") return 2;

    const auto missing = bl::csharp::probeManagedRuntime("/invalid/managed/probe.dll");
    if (!missing.attempted || missing.phase != "invalid-assembly" ||
        missing.libraryLoaded || missing.methodExecuted ||
        saved().find("phase=invalid-assembly") == std::string::npos) return 3;

    const auto inspected = bl::csharp::inspectManagedRuntime();
    if (!inspected.libraryLoaded || !inspected.symbolsReady ||
        inspected.domainInitialized || inspected.methodExecuted ||
        inspected.phase != "symbols-ready" ||
        saved().find("phase=symbols-ready") == std::string::npos) return 7;

    const auto first = bl::csharp::probeManagedRuntime(argv[1]);
    if (!first.libraryLoaded || !first.symbolsReady ||
        !first.domainInitialized || !first.methodExecuted ||
        first.phase != "verified" || first.result != 42) {
        std::fprintf(stderr, "phase=%s error=%s\n", first.phase.c_str(), first.error.c_str());
        return 4;
    }
    const auto second = bl::csharp::probeManagedRuntime(argv[1]);
    if (!second.methodExecuted || second.result != 42 ||
        saved().find("phase=verified") == std::string::npos ||
        saved().find("result=42") == std::string::npos) return 5;
    std::remove(checkpoint.c_str());
    std::puts("BUNNY_MANAGED_BRIDGE_MOCK_PASS=YES");
    return 0;
}
