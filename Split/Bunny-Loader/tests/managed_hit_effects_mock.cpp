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
    auto enable = reinterpret_cast<void(*)()>(dlsym(mono, "test_enable_effect_mode"));
    assert(enable);
    enable();
    const auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    setManagedNpcBuffGameHookReady(true);
    const ManagedNpcSnapshot npc{320, 180, 400.5f, 250.25f, true};
    const ManagedPlayerSnapshot player{0, 400, 500, 360.5f, 230.75f, true};
    queueManagedLiveHit(1137, 12, 36, npc, player);
    const auto wait = [](int expected) {
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            auto status = managedLiveHitStatus();
            if (status.processed == expected && status.effects.requested >= expected)
                return status;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "V85 effect request timed out");
        return ManagedLiveHitStatus{};
    };
    const auto first = wait(1);
    assert(first.hitContext.v4Calls == 1 && first.hitContext.v4Successes == 1);
    assert(first.effects.requested == 1 && first.effects.applied == 0);
    assert(first.effects.queueDepth == 1 && first.effects.gameHookInstalled);
    ManagedNpcBuffCommand taken;
    assert(!takeManagedNpcBuffForNpc(7, 320, 400.5f, 250.25f, &taken));
    assert(!takeManagedNpcBuffForNpc(36, 321, 400.5f, 250.25f, &taken));
    assert(managedLiveHitStatus().effects.rejected == 1);
    assert(managedLiveHitStatus().effects.queueDepth == 0);
    queueManagedLiveHit(1137, 12, 36, npc, player);
    const auto second = wait(2);
    assert(second.effects.requested == 2);
    assert(takeManagedNpcBuffForNpc(36, 320, 400.5f, 250.25f, &taken));
    assert(taken.buffId == 44 && taken.durationTicks == 180);
    noteManagedNpcBuffApplied(true);
    assert(managedLiveHitStatus().effects.applied == 1);
    assert(managedLiveHitStatus().effects.queueDepth == 0);
    queueManagedLiveHit(1137, 12, 36, npc, player);
    wait(3);
    pauseManagedLiveHit();
    assert(!takeManagedNpcBuffForNpc(36, 320, 400.5f, 250.25f, &taken));
    assert(managedLiveHitStatus().effects.stale >= 1);
    std::puts("CSHARP_V85_NPC_EFFECT_WORKER_GAME_THREAD=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
