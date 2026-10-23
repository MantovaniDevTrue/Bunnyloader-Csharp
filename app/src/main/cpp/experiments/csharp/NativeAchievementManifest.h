#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::achievement {

struct ConditionDescriptor {
    std::string key;
    int kind = 0;
    float maximum = 1;
    std::vector<int> contentTypes;
};

struct Descriptor {
    std::string name;
    std::string friendlyName;
    std::string description;
    std::string texture;
    int index = 0;
    int category = 0;
    bool hidden = false;
    bool completed = false;
    std::vector<ConditionDescriptor> conditions;
};

inline bool readNumber(std::string_view field, int* result) {
    if (!result || field.empty()) return false;
    const auto parsed = std::from_chars(field.data(), field.data() + field.size(), *result);
    return parsed.ec == std::errc{} && parsed.ptr == field.data() + field.size();
}

inline bool readDecimal(std::string_view text, float* output, bool zeroAllowed) {
    if (!output || text.empty() || text.size() > 48) return false;
    size_t i = 0;
    double mantissa = 0;
    int fraction = 0, digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        if (++digits > 24) return false;
        mantissa = mantissa * 10.0 + (text[i++] - '0');
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            if (++digits > 24) return false;
            mantissa = mantissa * 10.0 + (text[i++] - '0');
            ++fraction;
        }
    }
    if (digits == 0) return false;
    int exponent = 0, sign = 1;
    if (i < text.size() && (text[i] == 'E' || text[i] == 'e')) {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            sign = text[i++] == '-' ? -1 : 1;
        int exponentDigits = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            if (++exponentDigits > 3) return false;
            exponent = exponent * 10 + (text[i++] - '0');
        }
        if (exponentDigits == 0) return false;
    }
    if (i != text.size()) return false;
    exponent = exponent * sign - fraction;
    if (exponent < -75 || exponent > 75) return false;
    const double value = mantissa * std::pow(10.0, exponent);
    if (!std::isfinite(value) || value < 0 ||
        value > static_cast<double>(std::numeric_limits<float>::max()))
        return false;
    const float parsed = static_cast<float>(value);
    if (!std::isfinite(parsed) || (!zeroAllowed && parsed <= 0)) return false;
    *output = parsed;
    return true;
}

inline bool readFloat(std::string_view text, float* output) {
    return readDecimal(text, output, false);
}

inline bool readText(std::string_view hex, std::string* result, size_t limit, bool allowEmpty) {
    if (!result || hex.size() > limit * 2 || hex.size() % 2 || (!allowEmpty && hex.empty()))
        return false;
    std::string text;
    text.reserve(hex.size() / 2);
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = digit(hex[i]), lo = digit(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        const char c = static_cast<char>((hi << 4) | lo);
        if (static_cast<unsigned char>(c) < 32 || c == 127) return false;
        text.push_back(c);
    }
    *result = std::move(text);
    return true;
}

inline std::vector<std::string_view> split(std::string_view line, char separator) {
    std::vector<std::string_view> fields;
    for (;;) {
        const size_t index = line.find(separator);
        fields.push_back(line.substr(0, index));
        if (index == std::string_view::npos) break;
        line.remove_prefix(index + 1);
    }
    return fields;
}

inline bool parseManifest(std::string_view mod, std::string_view wire,
                          std::vector<Descriptor>* output) {
    if (!output || mod.empty() || mod.size() > 96 || wire.size() > 12000 ||
        wire.substr(0, 8) != "ACHMETA1" ||
        (wire.size() != 8 && wire[8] != '\n')) return false;
    std::vector<Descriptor> staging;
    if (wire.size() == 8) {
        output->clear();
        return true;
    }
    wire.remove_prefix(9);
    while (!wire.empty()) {
        const size_t end = wire.find('\n');
        const std::string_view line = wire.substr(0, end);
        if (line.empty()) return false;
        const auto fields = split(line, '|');
        if (fields[0] == "A") {
            if (fields.size() != 9 || staging.size() >= 128) return false;
            Descriptor entry;
            if (!readText(fields[1], &entry.name, 128, false) ||
                entry.name.substr(0, mod.size() + 1) != std::string(mod) + "/" ||
                !readText(fields[2], &entry.friendlyName, 256, false) ||
                !readText(fields[3], &entry.description, 512, true) ||
                !readText(fields[4], &entry.texture, 256, false) ||
                !readNumber(fields[5], &entry.index) || entry.index < 0 || entry.index > 255 ||
                !readNumber(fields[6], &entry.category) || entry.category < 0 || entry.category > 3 ||
                (fields[7] != "0" && fields[7] != "1") ||
                (fields[8] != "0" && fields[8] != "1") ||
                std::any_of(staging.begin(), staging.end(),
                    [&](const Descriptor& item) { return item.name == entry.name; }))
                return false;
            entry.hidden = fields[7] == "1";
            entry.completed = fields[8] == "1";
            staging.push_back(std::move(entry));
        } else if (fields[0] == "C") {
            if (fields.size() != 5 || staging.empty() ||
                staging.back().conditions.size() >= 32) return false;
            ConditionDescriptor condition;
            if (!readText(fields[1], &condition.key, 72, false) ||
                !readNumber(fields[2], &condition.kind) ||
                condition.kind < 0 || condition.kind > 6 ||
                !readFloat(fields[3], &condition.maximum)) return false;
            if (!fields[4].empty()) {
                for (std::string_view part : split(fields[4], ',')) {
                    int id = 0;
                    if (!readNumber(part, &id) || id < -65 || id > 32767 ||
                        condition.contentTypes.size() >= 1024) return false;
                    condition.contentTypes.push_back(id);
                }
            } else if (condition.kind >= 3) {
                return false;
            }
            if (std::any_of(staging.back().conditions.begin(),
                            staging.back().conditions.end(), [&](const ConditionDescriptor& c) {
                                return c.key == condition.key;
                            })) return false;
            staging.back().conditions.push_back(std::move(condition));
        } else return false;
        if (end == std::string_view::npos) break;
        wire.remove_prefix(end + 1);
    }
    if (staging.empty() ||
        std::any_of(staging.begin(), staging.end(),
            [](const Descriptor& entry) { return entry.conditions.empty(); })) return false;
    *output = std::move(staging);
    return true;
}

}
