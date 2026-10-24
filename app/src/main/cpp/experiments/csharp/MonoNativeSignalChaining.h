#pragma once
namespace bl::csharp {
using MonoSetSignalPolicy = void (*)(int);
// Preserve Unity/IL2CPP's handlers before Mono installs its own. With crash
// chaining OFF Mono gives a saved native handler first chance for native IPs;
// otherwise it runs native crash reporting before forwarding even handled GC faults.
inline bool configureMonoNativeSignalChaining(MonoSetSignalPolicy signal,
                                               MonoSetSignalPolicy crash) {
    if (!signal || !crash) return false;
    signal(1);
    crash(0);
    return true;
}
}
