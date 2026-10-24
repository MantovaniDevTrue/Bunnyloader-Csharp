#include "experiments/csharp/ManagedRuntime.h"
#include <signal.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>
namespace bl::csharp {
void rearmNativeAbortProbe(const char*) {}
bool installDestroyedMutexTrace() { return false; }
bool installNativeSegvTrace() { return false; }
}
static void owned(int) {}
static void ownedInfo(int, siginfo_t*, void*) {}
int main(int argc, char** argv) {
    assert(argc == 4);
    const int chosen = std::atoi(argv[2]);
    for (int sig : {SIGPWR, SIGXCPU, SIGTTIN}) {
        struct sigaction a{}; sigemptyset(&a.sa_mask); a.sa_handler = SIG_DFL;
        assert(sigaction(sig, &a, nullptr) == 0);
    }
    struct sigaction before{};
    if (chosen) {
        sigemptyset(&before.sa_mask);
        before.sa_handler = chosen < 0 ? SIG_IGN : owned;
        if (chosen == 999) { before.sa_flags = SA_SIGINFO; before.sa_sigaction = ownedInfo; }
        assert(sigaction(chosen == 999 ? SIGPWR : std::abs(chosen), &before, nullptr) == 0);
    }
    const std::string mode = argv[3];
    const auto result = mode == "DIRECT"
        ? bl::csharp::probeManagedRuntime(argv[1])
        : bl::csharp::probeManagedRuntimePinnedThread(argv[1], mode);
    void* lib = dlopen("libmonosgen-2.0.so", RTLD_NOW);
    assert(lib);
    auto chaining = reinterpret_cast<int(*)()>(dlsym(lib, "test_signal_chaining"));
    auto crashing = reinterpret_cast<int(*)()>(dlsym(lib, "test_crash_chaining"));
    assert(chaining && crashing && chaining() == 1 && crashing() == 0);
    auto count = reinterpret_cast<int(*)()>(dlsym(lib, "test_initializations")); assert(count);
    const bool preDomain = mode == "LIBRARY_ONLY" || mode == "PATH_ONLY" || mode == "SYSTEM_NATIVE_ONLY";
    if (chosen && !preDomain) {
        assert(result.phase == "mono-signal-conflict");
        assert(!result.domainInitialized && !result.methodExecuted && count() == 0);
        assert(!result.error.empty());
    } else if (preDomain) assert(count() == 0);
    else assert(result.domainInitialized && count() == 1);
    if (chosen) {
        struct sigaction after{};
        assert(sigaction(chosen == 999 ? SIGPWR : std::abs(chosen), nullptr, &after) == 0);
        assert(after.sa_handler == before.sa_handler);
        assert((after.sa_flags & SA_SIGINFO) == (before.sa_flags & SA_SIGINFO));
    }
    std::printf("PASS signal=%d mode=%s phase=%s\n", chosen, argv[3], result.phase.c_str());
    std::fflush(stdout);
    std::_Exit(0);
}
