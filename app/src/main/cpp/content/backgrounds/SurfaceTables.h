#pragma once
#include "il2cpp/PrimitiveArrayTransaction.h"
#include <array>

namespace bl::runtime::surface {
using TableTarget=il2cpp::PrimitiveArrayTarget;
// Called on the engine thread with direct, checked fields. All allocations,
// roots, owner/array checks, and the optional generation check precede writes.
// A getter/setter pair cannot provide this atomic publication contract.
bool growTables(const std::array<TableTarget,4>& targets,size_t total,std::string* error,
                bool(*isCurrent)(void*)=nullptr,void* context=nullptr);
}
