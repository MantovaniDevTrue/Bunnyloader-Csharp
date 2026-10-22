#pragma once
#include <atomic>
#include <cstdint>
#include <string>
namespace bl::csharp {
inline std::atomic<int64_t> universalLateCompletions{0};
inline void noteUniversalHookLateCompletion() {
    universalLateCompletions.fetch_add(1, std::memory_order_relaxed);
}
inline int64_t universalHookLateCompletionCount() {
    return universalLateCompletions.load(std::memory_order_relaxed);
}
bool universalHooksAccepting();
void pumpUniversalHookInstallsOnGameThread();
void disableUniversalHooks();
void pauseUniversalHooksForWorldExit();
void enableUniversalHooks();
bool registerUniversalHookMod(const std::string& id, const std::string& assembly);
void wakeUniversalHookWorker();
void pauseUniversalHookMods();
void resumeUniversalHookMods();
}
