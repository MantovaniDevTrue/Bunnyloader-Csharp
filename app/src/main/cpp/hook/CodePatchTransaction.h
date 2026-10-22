#pragma once
#include "il2cpp/Types.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace bl::runtime {
struct LoopLimitPatch {
    uint32_t before=0, after=0;
    bool exitOnEqual=false;
};
struct CodePatchResult {
    // Publication and page protection are separate facts. A failed RX restore
    // must not make a caller retain a count that no longer matches the code.
    bool applied=false;
    bool permissionsRestored=true;
    std::string detail;
};

// The caller must be on the engine thread, outside execution of the target.
// A mutex serializes patch writers; it does not suspend executing game threads.
// Every requested pattern must match exactly once, within a mapped RX range
// bounded by a following metadata method, before any code is made writable.
CodePatchResult patchLoopLimits(const MethodInfo* method,std::span<const LoopLimitPatch> patches);

// Retries only page protection left by an earlier OS failure; never rewrites
// instructions. New transactions refuse to proceed until this succeeds.
bool repairCodePatchProtection(std::string* error);
}
