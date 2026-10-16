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
    void* library = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    assert(library);
    auto legacyHits = reinterpret_cast<int(*)()>(dlsym(library, "test_legacy_hits"));
    auto v2Hits = reinterpret_cast<int(*)()>(dlsym(library, "test_v2_hits"));
    auto kills = reinterpret_cast<int(*)()>(dlsym(library, "test_kills"));
    assert(legacyHits && v2Hits && kills);

    auto init = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
    assert(init.methodExecuted && init.result == 42);
    std::string error;
    assert(enableManagedLiveHit("aa", argv[1], 1137, &error, true));
    assert(enableManagedLiveMod("bb", argv[1], 1138, "SecondBolt", &error));

    const auto wait = [](int processed) {
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            auto state = managedLiveHitStatus();
            if (state.processed == processed) {
                assert(state.failures == 0 && state.dropped == 0);
                return state;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false && "MonoVM V2 callback did not finish");
        return ManagedLiveHitStatus{};
    };

    queueManagedLiveHit(1137, 20, 137);
    auto first = wait(1);
    assert(v2Hits() == 1 && legacyHits() == 0);
    assert(first.lastResult == 159);
    assert(first.hitContext.v2Calls == 1);
    assert(first.hitContext.v2Successes == 1);
    assert(first.hitContext.lastNpcIndex == 137);
    assert(first.hitContext.uniqueNpcIndices == 1);
    assert(first.hitContext.legacyCalls == 0);

    queueManagedLiveHit(1138, 17);
    wait(2);
    assert(v2Hits() == 1 && legacyHits() == 1);
    assert(managedLiveHitStatus().hitContext.legacyCalls == 1);

    queueManagedLiveHit(1138, 19, 38);
    wait(3);
    assert(v2Hits() == 2 && legacyHits() == 1);
    const auto snapshot = managedLiveProjectileStatuses();
    assert(snapshot.size() == 1);
    assert(snapshot[0].hitContext.v2Calls == 1);
    assert(snapshot[0].hitContext.legacyCalls == 1);
    assert(snapshot[0].hitContext.lastNpcIndex == 38);
    assert(snapshot[0].hitContext.uniqueNpcIndices == 1);
    assert(managedLiveHitStatus().hitContext.v2Calls == 2);
    assert(managedLiveHitStatus().hitContext.uniqueNpcIndices == 2);

    queueManagedLiveKill(1137, 12);
    wait(4);
    assert(kills() == 1 && v2Hits() == 2);
    assert(managedLiveHitStatus().hitContext.v2Calls == 2);

    queueManagedLiveHit(1137, 11, 200);
    queueManagedLiveHit(1137, 11, -2);
    assert(managedLiveHitStatus().queued == 4);

    pauseManagedLiveHit();
    queueManagedLiveHit(1137, 11, 99);
    assert(v2Hits() == 2 && legacyHits() == 1 && kills() == 1);
    std::puts("CSHARP_V81_MONOVM_V2_LEGACY_KILL_DISPATCH=PASS");
    std::fflush(stdout);
    std::_Exit(0);
}
