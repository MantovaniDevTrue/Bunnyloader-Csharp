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
};

inline bool parseAchievementMetadata(const std::string& wire,
                                     const std::string& modUid,
                                     std::vector<AchievementMetadata>* records,
                                     std::string* error) {
    if (!records || !error) return false;
    records->clear();
    error->clear();
    constexpr std::string_view header = "BACH_META_V1\n";
    if (wire.size() < header.size() || wire.size() > 512 * 1024 ||
        wire.compare(0, header.size(), header) != 0 ||
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
    const auto safeText = [](const std::string& value, std::size_t maxBytes) {
        if (value.size() > maxBytes) return false;
        for (unsigned char ch : value)
            if (ch < 32 || ch == 127) return false;
        return true;
    };

    std::set<std::string> identities;
    std::size_t begin = header.size();
    while (begin < wire.size()) {
        const std::size_t newline = wire.find('\n', begin);
        if (newline == std::string::npos) return fail("Missing metadata line terminator");
        const std::string_view line(wire.data() + begin, newline - begin);
        begin = newline + 1;
        if (line.empty()) continue;
        if (records->size() >= 256) return fail("Too many achievement entries");

        std::vector<std::string_view> columns;
        std::size_t cursor = 0;
        for (;;) {
            const auto pipe = line.find('|', cursor);
            if (pipe == std::string_view::npos) {
                columns.push_back(line.substr(cursor));
                break;
            }
            columns.push_back(line.substr(cursor, pipe - cursor));
            if (columns.size() > 9) break;
            cursor = pipe + 1;
        }
        if (columns.size() != 9) return fail("Achievement row needs nine columns");
        AchievementMetadata entry;
        if (!hex(columns[0], &entry.name) ||
            !hex(columns[1], &entry.title) ||
            !hex(columns[2], &entry.description) ||
            !hex(columns[3], &entry.texture))
            return fail("Invalid hex metadata");
        const std::string prefix = modUid + "/";
        if (!entry.name.starts_with(prefix) || entry.name.size() <= prefix.size() ||
            entry.name.size() > 256 || !safeText(entry.name, 256) ||
            !safeText(entry.title, 800) || entry.title.empty() ||
            !safeText(entry.description, 4000) ||
            !safeText(entry.texture, 800) || entry.texture.empty() ||
            entry.texture.front() == '/' || entry.texture.find("..") != std::string::npos ||
            entry.texture.find('\\') != std::string::npos ||
            !identities.insert(entry.name).second)
            return fail("Invalid achievement identity or text");

        int hidden = 0, completed = 0;
        if (!integer(columns[4], 0, 65535, &entry.index) ||
            !integer(columns[5], 0, 3, &entry.category) ||
            !integer(columns[6], 0, 1, &hidden) ||
            !integer(columns[7], 0, 1, &completed))
            return fail("Invalid achievement UI fields");
        entry.hidden = hidden != 0;
        entry.completed = completed != 0;

        auto fraction = columns[8];
        if (fraction.empty() || fraction.size() > 48)
            return fail("Progress fraction missing or too long");
        const auto parsed = std::from_chars(fraction.data(),
            fraction.data() + fraction.size(), entry.progress);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != fraction.data() + fraction.size() ||
            !std::isfinite(entry.progress) || entry.progress < 0 || entry.progress > 1)
            return fail("Invalid achievement progress fraction");
        if (entry.completed && entry.progress < 1)
            return fail("Completed achievement cannot have partial progress");
        records->push_back(std::move(entry));
    }
    if (records->empty()) return fail("Achievement metadata empty");
    return true;
}

}
