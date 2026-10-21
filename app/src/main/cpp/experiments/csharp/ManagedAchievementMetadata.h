#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp {

struct AchievementConditionMetadata {
    std::string key;
    int kind = 0;
    double maximum = 1;
    double value = 0;
};

struct AchievementMetadata {
    std::string name;
    std::string title;
    std::string description;
    std::string texture;
    int index = 0;
    int category = 0;
    bool hidden = false;
    bool completed = false;
    double progress = 0;
    std::vector<AchievementConditionMetadata> conditions;
};

inline bool parseAchievementMetadata(const std::string& wire,
                                     const std::string& modUid,
                                     std::vector<AchievementMetadata>* records,
                                     std::string* error) {
    if (!records || !error) return false;
    records->clear();
    error->clear();
    constexpr std::string_view headerV1 = "BACH_META_V1\n";
    constexpr std::string_view headerV2 = "BACH_META_V2\n";
    const bool v2 = wire.starts_with(headerV2);
    const bool v1 = wire.starts_with(headerV1);
    if ((!v1 && !v2) || wire.size() > 512 * 1024 ||
        modUid.empty() || modUid.size() > 96) {
        *error = "Invalid achievement metadata envelope";
        return false;
    }
    const auto fail = [&](const char* reason) {
        records->clear();
        *error = reason;
        return false;
    };
    const auto hex = [](std::string_view input, std::string* target) {
        if (input.size() > 4096 || input.size() % 2 != 0) return false;
        target->clear();
        target->reserve(input.size() / 2);
        for (std::size_t i = 0; i < input.size(); i += 2) {
            unsigned int byte = 0;
            for (std::size_t j = 0; j < 2; ++j) {
                const char ch = input[i + j];
                const int digit = ch >= '0' && ch <= '9' ? ch - '0' :
                    ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
                if (digit < 0) return false;
                byte = (byte << 4) | static_cast<unsigned int>(digit);
            }
            target->push_back(static_cast<char>(byte));
        }
        return true;
    };
    const auto integer = [](std::string_view token, int min, int max, int* output) {
        if (token.empty() || token.size() > 12) return false;
        int result = 0;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            result < min || result > max) return false;
        *output = result;
        return true;
    };
    const auto number = [](std::string_view token, double* output) {
        if (token.empty() || token.size() > 48) return false;
        const auto parsed = std::from_chars(token.data(),
            token.data() + token.size(), *output);
        return parsed.ec == std::errc{} &&
               parsed.ptr == token.data() + token.size() &&
               std::isfinite(*output);
    };
    const auto safeText = [](const std::string& value, std::size_t maxBytes) {
        if (value.size() > maxBytes) return false;
        for (unsigned char ch : value)
            if (ch < 32 || ch == 127) return false;
        return true;
    };
    std::set<std::string> identities;
    std::size_t begin = v2 ? headerV2.size() : headerV1.size();
    while (begin < wire.size()) {
        const std::size_t newline = wire.find('\n', begin);
        if (newline == std::string::npos) return fail("Missing metadata line terminator");
        const std::string_view line(wire.data() + begin, newline - begin);
        begin = newline + 1;
        if (line.empty()) continue;

        std::vector<std::string_view> columns;
        std::size_t cursor = 0;
        for (;;) {
            const auto pipe = line.find('|', cursor);
            if (pipe == std::string_view::npos) {
                columns.push_back(line.substr(cursor));
                break;
            }
            columns.push_back(line.substr(cursor, pipe - cursor));
            if (columns.size() > 10) break;
            cursor = pipe + 1;
        }

        if (v2 && columns[0] == "C") {
            if (columns.size() != 6 || records->empty())
                return fail("Condition row without achievement or incorrect columns");
            auto& entry = records->back();
            if (entry.conditions.size() >= 32)
                return fail("Too many achievement conditions");
            AchievementConditionMetadata condition;
            std::string owner;
            if (!hex(columns[1], &owner) || owner != entry.name ||
                !hex(columns[2], &condition.key) ||
                !safeText(condition.key, 96) || condition.key.empty())
                return fail("Invalid condition identity");
            for (const auto& existing : entry.conditions)
                if (existing.key == condition.key)
                    return fail("Duplicate achievement condition");
            if (!integer(columns[3], 0, 6, &condition.kind) ||
                !number(columns[4], &condition.maximum) ||
                !number(columns[5], &condition.value) ||
                condition.maximum <= 0 || condition.maximum > 1000000000 ||
                condition.value < 0 || condition.value > condition.maximum)
                return fail("Invalid condition kind or bounds");
            if (condition.kind != 2 &&
                (condition.maximum != std::floor(condition.maximum) ||
                 condition.value != std::floor(condition.value)))
                return fail("Integer condition has nonintegral value");
            if (condition.kind >= 3 || condition.kind == 0) {
                if (condition.maximum != 1)
                    return fail("Event or flag condition must have maximum one");
            }
            entry.conditions.push_back(std::move(condition));
            continue;
        }
        if (records->size() >= 256) return fail("Too many achievement entries");
        if (v2 && (columns.empty() || columns[0] != "A"))
            return fail("Expected achievement row");
        const std::size_t prefix = v2 ? 1 : 0;
        if (columns.size() != 9 + prefix)
            return fail("Achievement row has wrong number of columns");
        AchievementMetadata entry;
        if (!hex(columns[prefix], &entry.name) ||
            !hex(columns[1 + prefix], &entry.title) ||
            !hex(columns[2 + prefix], &entry.description) ||
            !hex(columns[3 + prefix], &entry.texture))
            return fail("Invalid hex metadata");
        const std::string expected = modUid + "/";
        if (!entry.name.starts_with(expected) ||
            entry.name.size() <= expected.size() || entry.name.size() > 256 ||
            !safeText(entry.name, 256) || !safeText(entry.title, 800) ||
            entry.title.empty() || !safeText(entry.description, 4000) ||
            !safeText(entry.texture, 800) || entry.texture.empty() ||
            entry.texture.front() == '/' ||
            entry.texture.find("..") != std::string::npos ||
            entry.texture.find('\\') != std::string::npos ||
            !identities.insert(entry.name).second)
            return fail("Invalid achievement identity or text");
        int hidden = 0, completed = 0;
        if (!integer(columns[4 + prefix], 0, 65535, &entry.index) ||
            !integer(columns[5 + prefix], 0, 3, &entry.category) ||
            !integer(columns[6 + prefix], 0, 1, &hidden) ||
            !integer(columns[7 + prefix], 0, 1, &completed) ||
            !number(columns[8 + prefix], &entry.progress) ||
            entry.progress < 0 || entry.progress > 1)
            return fail("Invalid achievement UI fields or progress");
        entry.hidden = hidden != 0;
        entry.completed = completed != 0;
        if (entry.completed && entry.progress < 1)
            return fail("Completed achievement cannot have partial progress");
        if (v1) {
            entry.conditions.push_back(
                {"BunnyProgress", 1, 1000000, entry.progress * 1000000});
        } else if (!records->empty() && records->back().conditions.empty()) {
            return fail("Achievement has no condition rows");
        }
        records->push_back(std::move(entry));
    }
    if (records->empty()) return fail("Achievement metadata empty");
    if (v2 && records->back().conditions.empty())
        return fail("Achievement has no condition rows");
    for (const auto& entry : *records) {
        bool fullyCompleted = true;
        for (const auto& condition : entry.conditions)
            fullyCompleted &= condition.value >= condition.maximum;
        if (entry.completed != fullyCompleted)
            return fail("Achievement completion disagrees with condition values");
    }
    return true;
}

}
