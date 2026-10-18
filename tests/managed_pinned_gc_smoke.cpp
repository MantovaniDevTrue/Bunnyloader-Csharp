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
        assert(value("test_entries") == 0);
        assert(value("test_exits") == 0);
    };
    const std::string stage = argv[2];
    auto result = bl::csharp::probeManagedRuntimePinnedThread(argv[1], stage == "MISSING_API" ? "DOMAIN_ONLY" : stage);
    if (stage == "LIBRARY_ONLY" || stage == "PATH_ONLY" || stage == "SYSTEM_NATIVE_ONLY") {
        assert(!result.domainInitialized);
        assert(value("test_entries") == 0);
    } else {
        assert(result.domainInitialized);
        waitForIdle();
        if (stage == "DOMAIN_THREAD_AUDIT") {
            std::this_thread::sleep_for(std::chrono::milliseconds(2100));
            assert(value("test_owner_safe"));
        }
        for (int n = 0; n < 5; ++n) {
            result = bl::csharp::probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
            assert(result.methodExecuted && result.result == 42);
            waitForIdle();
        }
        assert(value("test_entries") == 0);
        assert(value("test_exits") == 0);
        assert(value("test_initializations") == 1);
        std::string error;
        const std::string testMod = "68c3115f-62f4-49f2-a56b-388256781d5f";
        assert(bl::csharp::enableManagedLiveHit(testMod, argv[1], 1136, &error));
        auto live = bl::csharp::managedLiveHitStatus();
        assert(live.enabled && live.phase == "armed");
        for (int attempt = 0; attempt < 10 && live.queued == 0; ++attempt) {
            bl::csharp::queueManagedLiveHit(1136, 17);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            live = bl::csharp::managedLiveHitStatus();
        }
        assert(live.queued == 1);
        const auto eventDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (live.processed == 0 && std::chrono::steady_clock::now() < eventDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            live = bl::csharp::managedLiveHitStatus();
        }
        assert(live.processed == 1);
        assert(live.lastDamage == 17);
        assert(live.lastResult == 18);
        assert(live.failures == 0);
        assert(value("test_entries") == 0 && value("test_exits") == 0);
        live = bl::csharp::pauseManagedLiveHit();
        assert(!live.enabled && live.phase == "paused");
        bl::csharp::queueManagedLiveHit(1136, 30);
        assert(bl::csharp::managedLiveHitStatus().queued == 1);
        assert(bl::csharp::enableManagedLiveHit(testMod, argv[1], 1136, &error, true));
        live = bl::csharp::managedLiveHitStatus();
        assert(live.automatic && live.enabled && live.phase == "armed");
        auto waitEvent = [&](int expected) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (std::chrono::steady_clock::now() < until) {
                live = bl::csharp::managedLiveHitStatus();
                if (live.processed >= expected) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            assert(false);
        };
        for (int i = 0; i < 10 && live.queued == 0; ++i) {
            bl::csharp::queueManagedLiveHit(1136, 17);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            live = bl::csharp::managedLiveHitStatus();
        }
        waitEvent(1);
        assert(live.hitProcessed == 1 && live.killProcessed == 0);
        assert(live.lastEvent == "OnHitNPC" && live.lastDamage == 17);
        assert(live.lastResult == 19);
        for (int i = 0; i < 10 && live.queued < 2; ++i) {
            bl::csharp::queueManagedLiveKill(1136, 18);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            live = bl::csharp::managedLiveHitStatus();
        }
        waitEvent(2);
        assert(live.hitProcessed == 1 && live.killProcessed == 1);
        assert(live.lastEvent == "OnKill" && live.lastDamage == 18);
        assert(live.lastResult == 21 && live.failures == 0);
        live = bl::csharp::pauseManagedLiveHit();
        assert(!live.enabled && live.phase == "paused");
        bl::csharp::queueManagedLiveKill(1136, 30);
        assert(bl::csharp::managedLiveHitStatus().queued == 2);
        assert(value("test_entries") == 0 && value("test_exits") == 0);
        const std::vector<std::pair<int, std::string>> npcs{{697, "MockNpc"}};
        const std::vector<std::pair<int, std::string>> projectiles{{1136, "MockProjectile"}};
        assert(bl::csharp::registerManagedEntitySourceMod(
            "68c3115f-62f4-49f2-a56b-388256781d5f", argv[1], npcs, projectiles));
        assert(bl::csharp::managedEntitySourceModsRegistered());
        assert(bl::csharp::hasManagedEntityNpcType(697));
        assert(bl::csharp::hasManagedEntityProjectileType(1136));
        assert(!bl::csharp::hasManagedEntityNpcType(-1));
        assert(!bl::csharp::hasManagedEntityProjectileType(32768));
        bl::csharp::ManagedEntityVelocity movement{};
        for (int i = 0; i < 18; i++) {
            bl::csharp::tickManagedEntityNpc(697, 3, 1, uintptr_t(1234),
                25, 25, 100.f, 100.f, 1.f, 0.f, 120.f, 100.f, &movement);
            bl::csharp::tickManagedEntityProjectile(1136, 4, 1, 0, uintptr_t(5678),
                100.f, 100.f, 2.f, 0.f, 120.f, 100.f, &movement);
        }
        bl::csharp::pauseManagedEntitySourceCallbacks();
        assert(bl::csharp::hasManagedEntityNpcType(697));
        assert(bl::csharp::hasManagedEntityProjectileType(1136));
        assert(value("test_entries") == 0 && value("test_exits") == 0);
    }
    std::printf("PASS pinned GC lifecycle: %s\n", argv[2]);
    std::fflush(stdout);
    std::_Exit(0);
}
