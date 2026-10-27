#pragma once

#include <string>
#include <string_view>

namespace bl::csharp {

inline bool validPrefixItemTooltip(std::string_view value) {
    if (value.empty() || value.size() > 120) return false;
    bool variable = false;
    for (size_t i = 0; i < value.size();) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch < 32 || ch == 127) return false;
        if (value[i] == '{') {
            size_t tokenLength = 0;
            const auto remaining = value.substr(i);
            if (remaining.starts_with("{damage}")) tokenLength = 8;
            else if (remaining.starts_with("{mana}")) tokenLength = 6;
            else if (remaining.starts_with("{stack}")) tokenLength = 7;
            if (!tokenLength) return false;
            variable = true;
            i += tokenLength;
        } else {
            if (value[i] == '}') return false;
            ++i;
        }
    }
    return variable;
}

inline std::string renderPrefixItemTooltip(
    std::string_view value, int damage, int mana, int stack) {
    if (!validPrefixItemTooltip(value)) return {};
    std::string output;
    output.reserve(value.size() + 24);
    for (size_t i = 0; i < value.size();) {
        const auto remaining = value.substr(i);
        if (remaining.starts_with("{damage}")) {
            output += std::to_string(damage);
            i += 8;
        } else if (remaining.starts_with("{mana}")) {
            output += std::to_string(mana);
            i += 6;
        } else if (remaining.starts_with("{stack}")) {
            output += std::to_string(stack);
            i += 7;
        } else {
            output += value[i++];
        }
        if (output.size() > 180) return {};
    }
    return output;
}

}
