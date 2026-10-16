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

int main(int argc, char** argv) {
    using namespace bl::csharp;
    assert(argc == 2);
    void* mono = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    assert(mono);
    auto count = [&](const char* name) {
        auto method = reinterpret_cast<int(*)()>(dlsym(mono, name));
        assert(method);
        return method();
    };
    const auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    assert(enableManagedLiveMod("bb", argv[1], 1138, "SecondBolt", &error));
    auto wait = [](int target) {
        const auto until = std::chrono::steady_clock::now() +
            std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < until) {
            const auto status = managedLiveHitStatus();
            if (status.processed == target) {
                assert(status.failures == 0 && status.dropped == 0);
                return status;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "V3 callback timed out");
        return ManagedLiveHitStatus{};
    };
    const ManagedNpcSnapshot npc{697, 120, 50.5f, 70.25f, true};
    queueManagedLiveHit(1137, 12, 43, npc);
    const auto first = wait(1);
    assert(first.lastResult == 5994);
    assert(count("test_v3_hits") == 1 && count("test_v2_hits") == 0);
    assert(first.hitContext.v3Calls == 1 && first.hitContext.v3Successes == 1);
    assert(first.hitContext.lastNpcType == 697 && first.hitContext.lastNpcLife == 120);
    assert(first.hitContext.lastNpcX == 50.5f && first.hitContext.lastNpcY == 70.25f);
    assert(first.hitContext.lastNpcIndex == 43);

    queueManagedLiveHit(1137, 20, 137);
    const auto second = wait(2);
    assert(count("test_v3_hits") == 1 && count("test_v2_hits") == 1);
    assert(second.hitContext.v2Calls == 1 && second.hitContext.v3Calls == 1);

    queueManagedLiveHit(1138, 13);
    wait(3);
    assert(count("test_legacy_hits") == 1);
    const auto secondMod = managedLiveProjectileStatuses();
    assert(secondMod.size() == 1 && secondMod[0].hitContext.legacyCalls == 1);

    queueManagedLiveHit(1138, 17, 29, {44, 90, 100.0f, 101.0f, true});
    const auto fourth = wait(4);
    assert(fourth.hitContext.v3Calls == 2);
    assert(count("test_v3_hits") == 2);
    assert(secondMod[0].hitContext.v3Calls == 0);
    const auto modAfter = managedLiveProjectileStatuses();
    assert(modAfter[0].hitContext.v3Calls == 1);
    assert(modAfter[0].hitContext.lastNpcType == 44);

    queueManagedLiveKill(1137, 9);
    wait(5);
    assert(count("test_kills") == 1);
    assert(managedLiveHitStatus().hitContext.v3Calls == 2);
    std::puts("CSHARP_V83_MANAGED_NPC_SNAPSHOT=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
