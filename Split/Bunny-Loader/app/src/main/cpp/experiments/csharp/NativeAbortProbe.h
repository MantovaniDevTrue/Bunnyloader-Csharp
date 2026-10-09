#pragma once

#include <string>

namespace bl::csharp {

bool installNativeAbortProbe(const std::string& directory,
                             const std::string& fileName = "native-abort-trace.txt");
void rearmNativeAbortProbe(const char* stage);
std::string currentNativeAbortHandler();
bool installDestroyedMutexTrace();
bool installNativeSegvTrace();
bool persistGameNativeModuleMaps(const std::string& directory);

}
