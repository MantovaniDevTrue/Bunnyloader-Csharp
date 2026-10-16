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
    setManagedCheckpointPath(argv[1]);
    void* library = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    auto block = reinterpret_cast<void(*)()>(dlsym(library, "test_block_modify"));
    auto entered = reinterpret_cast<bool(*)()>(dlsym(library, "test_modify_entered"));
    auto release = reinterpret_cast<void(*)()>(dlsym(library, "test_release_modify"));
    auto calls = reinterpret_cast<int(*)()>(dlsym(library, "test_modify_calls"));
    assert(block && entered && release && calls);
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    assert(registerManagedPlayerHurtMod("aa", argv[1]));
    auto modify = [](ManagedHurtResult* result) {
        return modifyManagedPlayerHurt("aa", 80, 1, 0, 100, 100, 0,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, result);
    };
    ManagedHurtResult initial;
    while (!modify(&initial)) {
        assert(std::chrono::steady_clock::now() < until);
        std::this_thread::yield();
    }
    assert(initial.damage == 40);
    const auto baseline = managedHurtSchedulerStatus();
    const int before = calls();
    block();
    ManagedHurtResult first{777}, second{888};
    std::thread caller([&] { assert(!modify(&first)); });
    while (!entered()) {
        assert(std::chrono::steady_clock::now() < until);
        std::this_thread::yield();
    }
    assert(!modify(&second));
    caller.join();
    assert(first.damage == 777 && second.damage == 888);
    release();
    while (managedHurtSchedulerStatus().lateResults == baseline.lateResults) {
        assert(std::chrono::steady_clock::now() < until);
        std::this_thread::yield();
    }
    const auto expired = managedHurtSchedulerStatus();
    assert(expired.timedOut >= baseline.timedOut + 2);
    assert(expired.cancelledBeforeRun >= baseline.cancelledBeforeRun + 1);
    assert(calls() == before + 1 && expired.pending == 0);
    ManagedHurtResult recovered;
    assert(modify(&recovered) && recovered.damage == 40);
    pauseManagedPlayerHurtMods();
    assert(!modify(&recovered));
    assert(registerManagedPlayerHurtMod("aa", argv[1]));
    assert(modify(&recovered) && recovered.damage == 40);
    std::puts("BUNNY_HURT_DEADLINE_CANCEL_LATE_AND_REENTRY=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
