#pragma once
#include <string>
#if defined(__ANDROID__) || defined(BUNNY_TEST_ANDROID_SIGNALS)
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#endif

namespace bl::csharp {
inline std::string monoAndroidSignalConflict([[maybe_unused]] bool alternativeSignals = false) {
#if defined(__ANDROID__) || defined(BUNNY_TEST_ANDROID_SIGNALS)
    if (alternativeSignals) {
        int available = 0;
        for (int sig = SIGRTMIN + 1; sig < SIGRTMAX; ++sig) {
            struct sigaction current{};
            if (sigaction(sig, nullptr, &current) != 0)
                return std::string("Cannot inspect real-time signal: ") + std::strerror(errno);
            if (current.sa_handler == SIG_DFL) ++available;
        }
        if (available >= 3) return {};
        return "Mono ARM64 needs three unused real-time signals before domain initialization";
    }
    const int signals[] = {SIGPWR, SIGXCPU, SIGTTIN};
    const char* names[] = {"SIGPWR", "SIGXCPU", "SIGTTIN"};
    for (unsigned i = 0; i < 3; ++i) {
        struct sigaction current{};
        if (sigaction(signals[i], nullptr, &current) != 0)
            return std::string("Cannot inspect ") + names[i] + ": " + std::strerror(errno);
        if (current.sa_handler == SIG_DFL) continue;
        std::string owner = "existing process handler";
        if (current.sa_handler == SIG_IGN) owner = "SIG_IGN";
        else {
            const void* address = current.sa_flags & SA_SIGINFO
                ? reinterpret_cast<const void*>(current.sa_sigaction)
                : reinterpret_cast<const void*>(current.sa_handler);
            Dl_info info{};
            if (dladdr(address, &info) && info.dli_fname) owner = info.dli_fname;
        }
        return std::string(names[i]) + " already owned by " + owner +
            "; Mono domain was not initialized. Use the isolated managedProbe worker.";
    }
#endif
    return {};
}
}
