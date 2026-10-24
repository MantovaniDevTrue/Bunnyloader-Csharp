#pragma once

#include <string>
#include <cstdint>

namespace bl::csharp {

// Lock-free breadcrumbs: no logging, allocation or file IO per frame.
void setNativeGameStage(uint32_t stage);
void setNativeManagedHookSlot(int slot);
uint32_t nativeGameStage();
int nativeManagedHookSlot();

bool installNativeAbortProbe(const std::string& directory,
                             const std::string& fileName = "native-abort-trace.txt");
void rearmNativeAbortProbe(const char* stage);
std::string currentNativeAbortHandler();
bool installDestroyedMutexTrace();
bool installNativeProcessExitTrace();
bool installNativeSegvTrace();
bool persistGameNativeModuleMaps(const std::string& directory);

}
