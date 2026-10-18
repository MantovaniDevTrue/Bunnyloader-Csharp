#include "experiments/csharp/ManagedRuntime.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>
namespace bl::csharp {
void rearmNativeAbortProbe(const char*) {}
bool installDestroyedMutexTrace() { return false; }
bool installNativeSegvTrace() { return false; }
}
using namespace bl::csharp;
int main(int argc, char** argv) {
    assert(argc == 2);
    void* lib = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    assert(lib);
    auto fail = reinterpret_cast<void(*)()>(dlsym(lib, "test_fail_next"));
    assert(fail);
    auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    assert(enableManagedLiveMod("bb", argv[1], 1138, "SecondBolt", &error));
    auto wait = [](int failures, int processed) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < until) {
            auto status = managedLiveHitStatus();
            if (status.failures == failures && status.processed == processed) return status;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "worker completion timeout");
        return ManagedLiveHitStatus{};
    };
    for (int i = 1; i <= 3; ++i) {
        fail(); queueManagedLiveHit(1138, 20); wait(i, 0);
    }
    auto bindings = managedLiveProjectileStatuses();
    assert(bindings.size() == 1 && !bindings[0].enabled && bindings[0].failures == 3);
    assert(managedLiveHitStatus().enabled);
    fail(); queueManagedLiveHit(1137, 20);
    assert(wait(4, 0).enabled);
    queueManagedLiveHit(1137, 20);
    assert(wait(4, 1).enabled);
    fail(); queueManagedLiveKill(1137, 20);
    assert(wait(5, 1).enabled);
    fail(); queueManagedLiveHit(1137, 20);
    assert(!wait(6, 1).enabled);
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    fail(); queueManagedLiveHit(1137, 20);
    assert(wait(1, 0).enabled);
    std::puts("MANAGED_FAILURE_BUDGET_ISOLATION=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
