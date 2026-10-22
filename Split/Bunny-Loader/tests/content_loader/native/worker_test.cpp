#include "experiments/csharp/ManagedRuntime.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
namespace bl::csharp {
void rearmNativeAbortProbe(const char*) {}
bool installDestroyedMutexTrace() { return false; }
bool installNativeSegvTrace() { return false; }
}
int main(int argc, char** argv) {
    using namespace bl::csharp;
    using namespace std::chrono_literals;
    assert(argc == 3);
    void* library = dlopen("libmonosgen-2.0.so", RTLD_NOW); assert(library);
    auto value = [&](const char* symbol) { auto f = reinterpret_cast<int(*)()>(dlsym(library, symbol)); assert(f); return f(); };
    auto call = [&](const char* symbol) { auto f = reinterpret_cast<void(*)()>(dlsym(library, symbol)); assert(f); f(); };
    auto entered = [&] {
        auto deadline = std::chrono::steady_clock::now() + 2s;
        while (!value("test_content_entered") && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        assert(value("test_content_entered"));
    };
    setManagedCheckpointPath(std::string(argv[1]) + ".checkpoint");
    std::string error;
    assert(!loadManagedContentMod("not-started", argv[1], &error));
    if (std::string(argv[2]) == "no-worker") {
        assert(shutdownManagedContent());
        assert(!loadManagedContentMod("closed", argv[1], &error));
    } else {
        auto state = probeManagedRuntimePinnedThread(argv[1], "PINNED_MONO");
        assert(state.methodExecuted && state.result == 42);
        assert(!loadManagedContentMod("", argv[1], &error));
        assert(!loadManagedContentMod("valid", argv[1], &error, 0));
        assert(!shutdownManagedContent(0)); // Invalid deadlines must not close the runtime.
        assert(loadManagedContentMod("normal", argv[1], &error));
        assert(value("test_content_active") == 1);
        call("test_block_content");
        std::thread duplicate([&] { std::string e; assert(!loadManagedContentMod("normal", argv[1], &e, 50)); });
        entered(); duplicate.join(); call("test_release_content");
        assert(loadManagedContentMod("normal", argv[1], &error));
        assert(value("test_content_active") == 1); // A lost duplicate ack must not unload an existing mod.
        assert(unloadManagedContentMod("normal", &error));
        assert(value("test_content_active") == 0);
        assert(!loadManagedContentMod("fail", argv[1], &error));
        assert(error.find("simulated managed") != std::string::npos);

        // A callback completes after its caller's deadline. The production worker must undo it
        // before running the next load; the mock checks observable ordering rather than queue internals.
        call("test_block_content");
        std::thread late([&] { std::string e; assert(!loadManagedContentMod("late", argv[1], &e, 50)); });
        entered(); late.join();
        call("test_release_content");
        assert(loadManagedContentMod("after-late", argv[1], &error));
        assert(value("test_content_after_late") == 1 && value("test_content_active") == 1);
        assert(unloadManagedContentMod("after-late", &error));

        // Saturate the real bounded queue behind an executing callback. Expired queued requests
        // must never invoke managed Load after they leave their caller's deadline.
        call("test_block_content");
        std::thread blocker([&] { std::string e; assert(loadManagedContentMod("blocker", argv[1], &e, 3000)); });
        entered();
        int before = value("test_content_loads");
        std::atomic<int> rejected{0};
        std::vector<std::thread> callers;
        for (int i = 0; i < 36; ++i) callers.emplace_back([&, i] {
            std::string e; assert(!loadManagedContentMod("queued-" + std::to_string(i), argv[1], &e, 100));
            if (e.find("queue full") != std::string::npos) ++rejected;
        });
        for (auto& thread : callers) thread.join();
        assert(rejected >= 4);
        call("test_release_content"); blocker.join();
        assert(unloadManagedContentMod("blocker", &error)); // Serial acknowledgement drains cancelled requests first.
        assert(value("test_content_loads") == before);

        // Shutdown is terminal and survives a caller timeout; an active load is rolled back
        // before global teardown, and no later admission can reactivate the assembly.
        call("test_block_content");
        std::thread shutdownLoad([&] { std::string e; assert(!loadManagedContentMod("shutdown-load", argv[1], &e, 3000)); });
        entered();
        assert(!shutdownManagedContent(10));
        assert(!loadManagedContentMod("too-late", argv[1], &error));
        call("test_release_content"); shutdownLoad.join();
        assert(shutdownManagedContent(2000));
        assert(value("test_content_shutdowns") == 1 && value("test_content_active") == 0);
        assert(value("test_content_wrong_thread") == 0);
        assert(value("test_entries") == 0 && value("test_exits") == 0);
    }
    std::puts("MANAGED_CONTENT_REAL_PINNED_QUEUE_ACK_TIMEOUT_CANCEL_ROLLBACK_SHUTDOWN_PASS");
    std::fflush(stdout);
    std::_Exit(0); // The production owner is intentionally persistent until process termination.
}
