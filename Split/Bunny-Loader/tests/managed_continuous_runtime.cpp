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
    assert(probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO").methodExecuted);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    assert(enableManagedLiveMod("bb", argv[1], 1138, "SecondBolt", &error));
    auto wait = [](int processed, int failures) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < until) {
            auto status = managedLiveHitStatus();
            if (status.processed == processed && status.failures == failures) return status;
            std::this_thread::yield();
        }
        assert(false && "worker timeout");
        return ManagedLiveHitStatus{};
    };
    for (int i = 0; i < 75000; i += 8) {
        for (int j = 0; j < 8; ++j) queueManagedLiveHit(j % 2 ? 1138 : 1137, 20);
        wait(i + 8, 0);
    }
    auto status = managedLiveHitStatus();
    assert(status.enabled && status.eventLimit == 0 && status.processed == 75000 && status.dropped == 0);
    for (int i = 0; i < 30; ++i)
        assert(enableManagedLiveProjectile("aa", argv[1], 1139 + i, "Bolt" + std::to_string(i), &error));
    assert(!enableManagedLiveProjectile("aa", argv[1], 1200, "OverflowBolt", &error));
    void* library = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    auto fail = reinterpret_cast<void(*)()>(dlsym(library, "test_fail_next"));
    assert(fail);
    for (int i = 1; i <= 3; ++i) {
        fail(); queueManagedLiveHit(1137, 20); wait(75000, i);
    }
    assert(!managedLiveHitStatus().primaryEnabled && managedLiveHitStatus().enabled);
    queueManagedLiveHit(1138, 20);
    assert(wait(75001, 3).enabled);
    assert(pauseManagedLiveMod("bb", &error));
    assert(resumeManagedLiveMod("bb", &error));
    queueManagedLiveKill(1138, 20);
    assert(wait(75002, 3).enabled);
    pauseManagedLiveHit();
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    queueManagedLiveHit(1137, 20);
    assert(wait(1, 0).enabled);
    std::puts("BUNNY_CONTINUOUS_RUNTIME_75000_PRIMARY_FAULT_ISOLATION=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
