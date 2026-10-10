#include "experiments/csharp/ManagedRuntime.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <thread>
namespace bl::csharp {
void rearmNativeAbortProbe(const char*) {}
bool installDestroyedMutexTrace() { return false; }
bool installNativeSegvTrace() { return false; }
}
int main(int argc, char** argv) {
    assert(argc == 3);
    void* lib = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    assert(lib);
    auto value = [&](const char* name) { auto f = reinterpret_cast<int(*)()>(dlsym(lib, name)); assert(f); return f(); };
    auto waitForIdle = [&] {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!value("test_owner_safe") && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(value("test_owner_safe"));
        assert(value("test_entries") == value("test_exits") + 1);
    };
    const std::string stage = argv[2];
    auto result = bl::csharp::probeManagedRuntimePinnedThread(argv[1], stage == "MISSING_API" ? "DOMAIN_ONLY" : stage);
    if (stage == "MISSING_API") {
        assert(result.phase == "pinned-gc-api-missing");
        assert(value("test_initializations") == 0);
    } else if (stage == "LIBRARY_ONLY" || stage == "PATH_ONLY" || stage == "SYSTEM_NATIVE_ONLY") {
        assert(!result.domainInitialized);
        assert(value("test_entries") == 0);
    } else {
        assert(result.domainInitialized);
        waitForIdle();
        if (stage == "DOMAIN_THREAD_AUDIT") {
            // Exercise the timed audit wake as well as the indefinite wait.
            std::this_thread::sleep_for(std::chrono::milliseconds(2100));
            assert(value("test_owner_safe"));
        }
        for (int n = 0; n < 5; ++n) {
            result = bl::csharp::probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
            assert(result.methodExecuted && result.result == 42);
            waitForIdle();
        }
        assert(value("test_exits") >= 5);
        assert(value("test_initializations") == 1);
    }
    std::printf("PASS pinned GC lifecycle: %s\n", argv[2]);
    std::fflush(stdout);
    // Production owner is intentionally immortal; do not destroy its mutexes.
    std::_Exit(0);
}
