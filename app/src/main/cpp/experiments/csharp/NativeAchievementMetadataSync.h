#pragma once

#include "experiments/csharp/ManagedAchievementMetadata.h"

#include <string>
#include <vector>

namespace bl::csharp {

inline bool updateAchievementMetadataSnapshot(
    std::vector<AchievementMetadata>* current,
    const std::vector<AchievementMetadata>& incoming,
    std::string* error) {
    if (!current || !error) return false;
    error->clear();
    if (current->size() != incoming.size()) {
        *error = "Achievement count changed after native registration";
        return false;
    }

    for (std::size_t i = 0; i < incoming.size(); ++i) {
        const auto& old = (*current)[i];
        const auto& next = incoming[i];
        if (old.name != next.name || old.title != next.title ||
            old.description != next.description ||
            old.texture != next.texture || old.index != next.index ||
            old.category != next.category ||
            old.conditions.size() != next.conditions.size() ||
            (old.completed && !next.completed)) {
            *error = "Static achievement metadata changed or completed achievement regressed";
            return false;
        }
        for (std::size_t j = 0; j < next.conditions.size(); ++j) {
            const auto& a = old.conditions[j];
            const auto& b = next.conditions[j];
            if (a.key != b.key || a.kind != b.kind || a.maximum != b.maximum ||
                (old.completed && b.value < a.value) ||
                (a.kind == 0 && b.value < a.value)) {
                *error = "Achievement condition identity changed or completed flag regressed";
                return false;
            }
        }
    }

    for (std::size_t i = 0; i < incoming.size(); ++i) {
        auto& old = (*current)[i];
        const auto& next = incoming[i];
        old.hidden = next.hidden;
        old.completed = next.completed;
        old.progress = next.progress;
        for (std::size_t j = 0; j < next.conditions.size(); ++j)
            old.conditions[j].value = next.conditions[j].value;
    }
    return true;
}

}
