#pragma once

#include "il2cpp/Types.h"

namespace bl::csharp {
bool installManagedPrefixGameplayHooks();
bool applyManagedPrefixViaNativeRoll(Il2CppObject* item, int prefix,
                                     const MethodInfo* itemPrefixMethod,
                                     bool* hookReached);
}
