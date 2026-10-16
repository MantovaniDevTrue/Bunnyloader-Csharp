#pragma once
#include <string>
namespace bl::csharp {
void pumpUniversalHookInstallsOnGameThread();
void disableUniversalHooks();
void enableUniversalHooks();
bool registerUniversalHookMod(const std::string& id, const std::string& assembly);
void wakeUniversalHookWorker();
void pauseUniversalHookMods();
void resumeUniversalHookMods();
}
