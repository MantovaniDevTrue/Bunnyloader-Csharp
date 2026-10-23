#pragma once

#include "experiments/csharp/NativeAchievementManifest.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::achievement {

struct ConditionState {
    std::string key;
    int kind = 0;
    float value = 0;
};

struct AchievementState {
    std::string name;
    bool hidden = false;
    bool completed = false;
    std::vector<ConditionState> conditions;
};

inline bool parseState(std::string_view mod, std::string_view wire,
                       const std::vector<Descriptor>& definitions,
                       std::vector<AchievementState>* output) {
    if (!output || wire.size() > 12000 || wire.substr(0, 9) != "ACHSTATE1" ||
        (wire.size() != 9 && wire[9] != '\n') ||
        definitions.size() > 128 || mod.empty() || mod.size() > 96)
        return false;
    if (definitions.empty()) {
        if (wire != "ACHSTATE1") return false;
        output->clear();
        return true;
    }
    if (wire.size() == 9) return false;
    wire.remove_prefix(10);
    std::vector<AchievementState> result;
    while (!wire.empty()) {
        size_t end = wire.find('\n');
        std::string_view line = wire.substr(0, end);
        if (line.empty()) return false;
        auto fields = split(line, '|');
        if (fields[0] == "S") {
            if (fields.size() != 4 || result.size() >= definitions.size()) return false;
            AchievementState state;
            if (!readText(fields[1], &state.name, 128, false) ||
                state.name != definitions[result.size()].name ||
                (fields[2] != "0" && fields[2] != "1") ||
                (fields[3] != "0" && fields[3] != "1"))
                return false;
            state.hidden = fields[2] == "1";
            state.completed = fields[3] == "1";
            result.push_back(std::move(state));
        } else if (fields[0] == "V") {
            if (fields.size() != 4 || result.empty()) return false;
            auto& state = result.back();
            const auto& original = definitions[result.size() - 1].conditions;
            if (state.conditions.size() >= original.size()) return false;
            ConditionState condition;
            if (!readText(fields[1], &condition.key, 72, false) ||
                !readNumber(fields[2], &condition.kind) ||
                condition.key != original[state.conditions.size()].key ||
                condition.kind != original[state.conditions.size()].kind)
                return false;
            const auto parsed = std::from_chars(fields[3].data(),
                fields[3].data() + fields[3].size(), condition.value);
            if (parsed.ec != std::errc{} || parsed.ptr != fields[3].data() + fields[3].size() ||
                !std::isfinite(condition.value) || condition.value < 0 ||
                condition.value > original[state.conditions.size()].maximum ||
                (condition.kind != 2 && std::trunc(condition.value) != condition.value))
                return false;
            state.conditions.push_back(std::move(condition));
        } else return false;
        if (end == std::string_view::npos) break;
        wire.remove_prefix(end + 1);
    }
    if (result.size() != definitions.size()) return false;
    for (size_t index = 0; index < result.size(); ++index) {
        const auto& entry = result[index];
        const auto& reference = definitions[index];
        if (entry.conditions.size() != reference.conditions.size()) return false;
        if (entry.completed && !std::equal(entry.conditions.begin(), entry.conditions.end(),
            reference.conditions.begin(), [](const ConditionState& state,
                                            const ConditionDescriptor& desc) {
                return state.value >= desc.maximum;
            })) return false;
    }
    *output = std::move(result);
    return true;
}

}
