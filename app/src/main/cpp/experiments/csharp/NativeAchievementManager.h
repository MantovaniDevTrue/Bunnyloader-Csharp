#pragma once

#include "experiments/csharp/NativeAchievementRegistrationPlan.h"
#include "il2cpp/Types.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bl::csharp::achievement {

class NativeAchievementManager {
    struct Entry {
        std::string name;
        uint32_t root = 0;
        std::vector<uint32_t> conditions;
    };
    struct Owner {
        std::string revision;
        std::vector<Entry> entries;
    };
    std::map<std::string, Owner> mods_;

public:
    bool install(const NativeRegistrationPlan& plan,
                 const std::vector<Descriptor>& definitions,
                 const std::vector<AchievementState>& states,
                 std::string* error);
    bool synchronize(const std::string& mod,
                     const std::vector<AchievementState>& states,
                     std::string* error);
    bool retire(const std::string& mod, std::string* error);
    bool owns(const std::string& mod) const;
};

NativeAchievementManager& nativeAchievementManager();

}
