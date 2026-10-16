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
    auto enable = reinterpret_cast<void(*)()>(dlsym(mono, "test_enable_impulse_mode"));
    assert(enable);
    enable();
    const auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    setManagedNpcBuffGameHookReady(true);
    const ManagedNpcSnapshot npc{320, 180, 400.5f, 250.25f, true};
    const ManagedPlayerSnapshot player{0, 400, 500, 360.5f, 230.75f, true};
    auto wait = [](int wanted) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < until) {
            auto s = managedLiveHitStatus();
            if (s.processed == wanted && s.effects.requested >= wanted) return s;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "managed impulse V4 timed out");
        return ManagedLiveHitStatus{};
    };
    queueManagedLiveHit(1137, 12, 36, npc, player);
    const auto first = wait(1);
    assert(first.hitContext.v4Calls == 1 && first.hitContext.v4Failures == 0);
    assert(first.effects.requested == 1 && first.effects.applied == 0);
    ManagedNpcBuffCommand command;
    assert(!takeManagedNpcBuffForNpc(35, 320, 400.5f, 250.25f, &command));
    assert(takeManagedNpcBuffForNpc(36, 320, 400.5f, 250.25f, &command));
    assert(command.impulseDirection == 2 && command.impulseTenths == 35);
    assert(command.buffId == 0 && command.durationTicks == 0);
    noteManagedNpcImpulseApplied(true);
    const auto applied = managedLiveHitStatus();
    assert(applied.effects.applied == 1 && applied.effects.impulsesApplied == 1);
    assert(applied.effects.lastImpulseDirection == 2);
    assert(applied.effects.lastImpulseTenths == 35);
    assert(applied.effects.queueDepth == 0);
    queueManagedLiveHit(1137, 12, 36, npc, player);
    wait(2);
    pauseManagedLiveHit();
    assert(!takeManagedNpcBuffForNpc(36, 320, 400.5f, 250.25f, &command));
    assert(managedLiveHitStatus().effects.stale >= 1);
    std::puts("CSHARP_V86_MANAGED_NPC_IMPULSE_COMMAND=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
