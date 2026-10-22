#pragma once

#include "il2cpp/Types.h"
#include <string>

namespace bl::csharp {

// Native BCL dictionary adapter, not a Terraria or tModLoader API.
// Runtime verification happens before any manager mutations. If the mobile
// build changes the private manager structure, operations fail closed.
class NativeAchievementManagerAdapter {
public:
    bool prepare(Il2CppObject* manager, std::string* error);
    bool removeExact(const std::string& name, Il2CppObject* expected,
                     std::string* error);
    bool containsExact(const std::string& name, Il2CppObject* expected,
                       std::string* error) const;
    bool isReady() const { return dictionary_ != nullptr; }

private:
    Il2CppObject* dictionary_ = nullptr;
    const MethodInfo* lookup_ = nullptr;
    const MethodInfo* remove_ = nullptr;
};

}
