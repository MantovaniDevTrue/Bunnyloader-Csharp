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
    std::map<std::string, std::vector<Entry>> compiled;
    std::map<std::string, std::vector<Entry>> managed;

    static bool equals(const std::vector<Entry>& left, const std::vector<Entry>& right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i)
            if (left[i].culture != right[i].culture ||
                left[i].key != right[i].key || left[i].value != right[i].value)
                return false;
        return true;
    }

    bool rebuild(const std::string& modId) {
        std::map<std::pair<std::string, std::string>, Entry> merged;
        const auto original = compiled.find(modId);
        if (original != compiled.end())
            for (const auto& entry : original->second)
                merged[{entry.key, entry.culture}] = entry;
        const auto overlay = managed.find(modId);
        if (overlay != managed.end())
            for (const auto& entry : overlay->second)
                merged[{entry.key, entry.culture}] = entry;
        std::vector<Entry> next;
        next.reserve(merged.size());
        std::set<std::string> keys;
        for (const auto& [identity, entry] : merged) {
            keys.insert(entry.key);
            next.push_back(entry);
        }
        const auto previous = state.active.find(modId);
        if (previous != state.active.end() && equals(previous->second, next))
            return false;
        if (previous == state.active.end() && next.empty()) return false;
        if (previous != state.active.end()) {
            std::set<std::string> otherKeys;
            for (const auto& [otherMod, entries] : state.active)
                if (otherMod != modId)
                    for (const auto& entry : entries)
                        otherKeys.insert(entry.key);
            for (const auto& entry : previous->second)
                if (!keys.contains(entry.key) && !otherKeys.contains(entry.key))
                    state.retired.insert(entry.key);
        }
        for (const auto& key : keys) state.retired.erase(key);
        if (next.empty()) state.active.erase(modId);
        else state.active[modId] = std::move(next);
        ++state.revision;
        return true;
    }

public:
    void replace(const std::string& modId, std::vector<Entry> rows) {
        managed.erase(modId);
        if (rows.empty()) compiled.erase(modId);
        else compiled[modId] = std::move(rows);
        rebuild(modId);
    }

    bool replaceDynamic(const std::string& modId, std::vector<Entry> rows) {
        if (rows.empty()) managed.erase(modId);
        else managed[modId] = std::move(rows);
        return rebuild(modId);
    }

    bool retainOnly(const std::set<std::string>& activeMods) {
        std::set<std::string> removed;
        for (auto it = compiled.begin(); it != compiled.end();) {
            if (activeMods.contains(it->first)) { ++it; continue; }
            removed.insert(it->first);
            it = compiled.erase(it);
        }
        for (auto it = managed.begin(); it != managed.end();) {
            if (activeMods.contains(it->first)) { ++it; continue; }
            removed.insert(it->first);
            it = managed.erase(it);
        }
        bool changed = false;
        for (const auto& id : removed) changed = rebuild(id) || changed;
        return changed;
    }

    uint64_t revision() const { return state.revision; }

    SessionSnapshot snapshot() const { return state; }
};
}
