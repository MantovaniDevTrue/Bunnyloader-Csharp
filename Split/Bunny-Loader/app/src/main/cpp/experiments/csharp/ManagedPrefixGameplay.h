#pragma once

#include "il2cpp/Types.h"
#include <string>

namespace bl::csharp {
bool installManagedPrefixGameplayHooks();
bool managedPrefixGameplayReady();
bool applyManagedPrefixStatFallback(Il2CppObject* item, int prefix, std::string* reason);
bool applyManagedPrefixViaNativeRoll(Il2CppObject* item, int prefix,
                                     const MethodInfo* itemPrefixMethod,
                                     bool* hookReached);
}
