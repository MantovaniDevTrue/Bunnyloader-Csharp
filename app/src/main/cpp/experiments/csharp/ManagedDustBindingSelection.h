#pragma once

#include "experiments/csharp/SourceLoader.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {

inline std::vector<std::pair<int, std::string>>
selectManagedDustBindings(const SourceModStatus& mod) {
    std::vector<std::pair<int, std::string>> types;
    if (mod.dusts <= 0) return types;
    types.reserve(static_cast<std::size_t>(mod.dusts));
    for (const auto& [key, nativeType] : mod.registeredKindTypes) {
        if (!key.starts_with("DUST:")) continue;
        const std::string name = key.substr(5);
        const auto mask = mod.dustMasks.find(name);
        if (mask == mod.dustMasks.end() || mask->second == 0u) continue;
        types.emplace_back(nativeType, name);
    }
    return types;
}

}
