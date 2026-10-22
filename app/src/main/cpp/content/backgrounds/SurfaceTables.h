#pragma once
#include "il2cpp/CheckedFieldWrite.h"
#include <array>

namespace bl::runtime::surface {
struct TableTarget {const il2cpp::CheckedFieldWrite* field=nullptr;void* owner=nullptr;};
// Called on the engine thread with direct, checked fields. All allocations,
// roots, owner/array checks, and the optional generation check precede writes.
// A getter/setter pair cannot provide this atomic publication contract.
bool growTables(const std::array<TableTarget,4>& targets,size_t total,std::string* error,
                bool(*isCurrent)(void*)=nullptr,void* context=nullptr);
}
