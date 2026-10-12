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
    const auto counter = [&](const char* name) {
        auto fn = reinterpret_cast<int(*)()>(dlsym(mono, name));
        assert(fn);
        return fn();
    };
    const auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    assert(enableManagedLiveMod("bb", argv[1], 1138, "SecondBolt", &error));
    const auto wait = [](int target) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto status = managedLiveHitStatus();
            if (status.processed == target) {
                assert(status.failures == 0 && status.dropped == 0);
                return status;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "V4 callback timed out");
        return ManagedLiveHitStatus{};
    };

    const ManagedNpcSnapshot npc{320, 180, 400.5f, 250.25f, true};
    const ManagedPlayerSnapshot player{0, 400, 500, 360.5f, 230.75f, true};
    queueManagedLiveHit(1137, 12, 26, npc, player);
    const auto first = wait(1);
    assert(counter("test_v4_hits") == 1);
    assert(first.hitContext.v4Calls == 1 && first.hitContext.v4Successes == 1);
    assert(first.hitContext.v3Calls == 0);
    assert(first.hitContext.lastNpcIndex == 26 && first.hitContext.lastNpcType == 320);
    assert(first.hitContext.lastPlayerIndex == 0 && first.hitContext.lastPlayerLife == 400);
    assert(first.hitContext.lastPlayerLifeMax == 500);
    assert(first.hitContext.lastPlayerX == 360.5f && first.hitContext.lastPlayerY == 230.75f);
    assert(first.hitContext.uniqueNpcIndices == 1);

    queueManagedLiveHit(1137, 9, 28, npc);
    const auto second = wait(2);
    assert(counter("test_v3_hits") == 1);
    assert(second.hitContext.v4Calls == 1 && second.hitContext.v3Calls == 1);

    queueManagedLiveHit(1137, 20, 30);
    const auto third = wait(3);
    assert(counter("test_v2_hits") == 1);
    assert(third.hitContext.v2Calls == 1);

    queueManagedLiveHit(1138, 13);
    wait(4);
    assert(counter("test_legacy_hits") == 1);

    queueManagedLiveHit(1138, 17, 29, npc, player);
    const auto fifth = wait(5);
    assert(fifth.hitContext.v4Calls == 2);
    const auto secondary = managedLiveProjectileStatuses();
    assert(secondary.size() == 1);
    assert(secondary[0].hitContext.v4Calls == 1);
    assert(secondary[0].hitContext.lastPlayerLife == 400);

    queueManagedLiveKill(1137, 9);
    const auto sixth = wait(6);
    assert(counter("test_kills") == 1);
    assert(sixth.hitContext.v4Calls == 2);
    assert(sixth.hitContext.invalidV2Indices == 0);
    std::puts("CSHARP_V84_PLAYER_NPC_CONTEXT=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
