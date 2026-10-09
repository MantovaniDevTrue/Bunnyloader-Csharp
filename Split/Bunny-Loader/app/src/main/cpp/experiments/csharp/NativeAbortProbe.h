#pragma once

#include <string>

namespace bl::csharp {

bool installNativeAbortProbe(const std::string& directory);
void rearmNativeAbortProbe(const char* stage);
std::string currentNativeAbortHandler();
bool installDestroyedMutexTrace();

}
