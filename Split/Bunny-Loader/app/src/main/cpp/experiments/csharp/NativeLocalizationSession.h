#pragma once
#include "experiments/csharp/NativeLocalizationRuntime.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::localization {

struct SessionSnapshot {
    std::map<std::string, std::vector<Entry>> active;
    std::set<std::string> retired;
    uint64_t revision = 0;
};

class NativeLocalizationSession {
    SessionSnapshot state;

public:
    void replace(const std::string& modId, std::vector<Entry> rows) {
        std::set<std::string> nextKeys;
        for (const auto& entry : rows)
            nextKeys.insert(entry.key);
        const auto old = state.active.find(modId);
        if (old != state.active.end())
            for (const auto& entry : old->second)
                if (!nextKeys.contains(entry.key))
                    state.retired.insert(entry.key);
        for (const auto& key : nextKeys)
            state.retired.erase(key);
        if (rows.empty())
            state.active.erase(modId);
        else
            state.active[modId] = std::move(rows);
        ++state.revision;
    }

    bool retainOnly(const std::set<std::string>& activeMods) {
        bool changed = false;
        for (auto it = state.active.begin(); it != state.active.end();) {
            if (activeMods.contains(it->first)) {
                ++it;
                continue;
            }
            for (const auto& entry : it->second)
                state.retired.insert(entry.key);
            it = state.active.erase(it);
            changed = true;
        }
        if (changed) ++state.revision;
        return changed;
    }

    SessionSnapshot snapshot() const {
        return state;
    }
};
}
