#pragma once

#include "experiments/csharp/NativeAchievementManifest.h"
#include "experiments/csharp/NativeAchievementState.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::achievement {

struct NativeRegistrationPlan {
    std::string owner;
    std::string revision;
    std::vector<std::string> names;
    bool prepared = false;
    std::string reason;
};

inline NativeRegistrationPlan prepareNativeRegistration(
    std::string_view owner, std::string_view revision,
    const std::vector<Descriptor>& descriptors,
    const std::vector<AchievementState>& current,
    bool reversible, bool textureReady) {
    NativeRegistrationPlan plan;
    if (owner.empty() || owner.size() > 96 || revision.empty() ||
        revision.size() > 4096 || descriptors.empty() || descriptors.size() > 128 ||
        current.size() != descriptors.size()) {
        plan.reason = "InvalidOwnerRevisionOrState";
        return plan;
    }
    plan.owner.assign(owner);
    plan.revision.assign(revision);
    if (!reversible) {
        plan.reason = "ReversibleUnregisterUnavailable";
        return plan;
    }
    if (!textureReady) {
        plan.reason = "AchievementAtlasUnavailable";
        return plan;
    }
    for (size_t i = 0; i < descriptors.size(); ++i) {
        const auto& def = descriptors[i];
        const auto& state = current[i];
        if (def.name != state.name ||
            def.name.substr(0, owner.size() + 1) != std::string(owner) + "/" ||
            def.conditions.empty() || def.conditions.size() != state.conditions.size() ||
            def.name.size() > 128 || def.friendlyName.empty() || def.texture.empty() ||
            def.index > 7 || def.index < 0 ||
            std::find(plan.names.begin(), plan.names.end(), def.name) != plan.names.end()) {
            plan.reason = "InvalidAchievementDefinition";
            return plan;
        }
        for (size_t j = 0; j < def.conditions.size(); ++j) {
            const auto& condition = def.conditions[j];
            const auto& value = state.conditions[j];
            if (condition.kind > 2 || condition.kind < 0 ||
                condition.key != value.key || condition.kind != value.kind ||
                value.value < 0 || value.value > condition.maximum) {
                plan.reason = "UnsupportedOrInconsistentCondition";
                return plan;
            }
        }
        plan.names.push_back(def.name);
    }
    plan.prepared = true;
    plan.reason = "Ready";
    return plan;
}

struct NativeRegistrationLedger {
    std::string owner;
    std::string revision;
    std::vector<std::string> entries;
    bool committed = false;

    bool begin(const NativeRegistrationPlan& plan) {
        if (!plan.prepared || committed || !entries.empty() || !owner.empty()) return false;
        owner = plan.owner;
        revision = plan.revision;
        return true;
    }

    bool add(std::string_view name) {
        if (committed || owner.empty() || name.substr(0, owner.size() + 1) != owner + "/" ||
            std::find(entries.begin(), entries.end(), name) != entries.end()) return false;
        entries.emplace_back(name);
        return true;
    }

    bool commit(const NativeRegistrationPlan& plan) {
        if (!plan.prepared || owner != plan.owner || revision != plan.revision ||
            entries != plan.names) return false;
        committed = true;
        return true;
    }

    void reset() {
        owner.clear();
        revision.clear();
        entries.clear();
        committed = false;
    }
};

}
