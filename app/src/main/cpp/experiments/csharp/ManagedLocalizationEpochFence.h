#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <string>

namespace bl::csharp {

class ManagedLocalizationEpochFence {
    std::map<std::string, uint64_t> floors;

public:
    bool permits(const std::string& modId, uint64_t epoch) const {
        if (modId.empty() || epoch == 0) return false;
        const auto it = floors.find(modId);
        return it == floors.end() || epoch >= it->second;
    }

    bool current(const std::string& modId, uint64_t epoch) const {
        const auto it = floors.find(modId);
        return epoch != 0 && it != floors.end() && epoch == it->second;
    }

    void activate(const std::string& modId, uint64_t epoch) {
        if (permits(modId, epoch)) floors[modId] = epoch;
    }

    void retire(const std::string& modId) {
        uint64_t& floor = floors[modId];
        if (floor < std::numeric_limits<uint64_t>::max()) ++floor;
    }
};
}
