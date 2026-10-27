#pragma once

#include "il2cpp/Types.h"
#include <string>
#include <cstdint>

namespace bl::csharp {
bool installManagedPrefixChanceHook();
bool installManagedPrefixGameplayHooks();
bool managedPrefixGameplayReady();

struct ManagedPrefixDiagnostics {
    uint64_t rollAttempts = 0;
    uint64_t restoredRolls = 0;
    uint64_t chosenItemRolls = 0;
    uint64_t preferredRolls = 0;
    uint64_t weightedRolls = 0;
    uint64_t vanillaFallbacks = 0;
    uint64_t rejectedRolls = 0;
    uint64_t totalRollMicroseconds = 0;
    uint64_t maxRollMicroseconds = 0;
};

ManagedPrefixDiagnostics managedPrefixDiagnostics();
bool applyManagedPrefixStatFallback(Il2CppObject* item, int prefix, std::string* reason);
bool applyManagedPrefixViaNativeRoll(Il2CppObject* item, int prefix,
                                     const MethodInfo* itemPrefixMethod,
                                     bool* hookReached);
}
