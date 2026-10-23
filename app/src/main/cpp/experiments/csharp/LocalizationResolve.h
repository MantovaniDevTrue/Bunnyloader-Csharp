#pragma once
#include <charconv>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace bl::csharp::localization {

using GameTextLookup = std::function<bool(const std::string&, std::string*)>;

inline bool localeReferenceChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/';
}

inline bool parseLocaleReference(std::string_view input, size_t start, size_t* end,
                                std::string* key, unsigned* offset) {
    if (!end || !key || !offset || start + 3 >= input.size() ||
        input[start] != '{' || input[start + 1] != '$') return false;
    size_t cursor = start + 2;
    while (cursor < input.size() && localeReferenceChar(input[cursor])) ++cursor;
    if (cursor == start + 2) return false;
    *key = std::string(input.substr(start + 2, cursor - start - 2));
    *offset = 0;
    if (cursor < input.size() && input[cursor] == '@') {
        const size_t first = ++cursor;
        while (cursor < input.size() && input[cursor] >= '0' && input[cursor] <= '9') ++cursor;
        if (first == cursor || cursor - first > 8) return false;
        const auto digits = input.substr(first, cursor - first);
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), *offset);
        if (result.ec != std::errc{} || *offset > 10000) return false;
    }
    if (cursor >= input.size() || input[cursor] != '}') return false;
    *end = cursor + 1;
    return true;
}

inline std::string shiftLocaleArguments(const std::string& input, unsigned offset) {
    if (!offset) return input;
    std::string output;
    output.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] != '{') {
            output += input[i++];
            continue;
        }
        size_t start = i + 1;
        const bool caret = start < input.size() && input[start] == '^';
        if (caret) ++start;
        size_t cursor = start;
        while (cursor < input.size() && input[cursor] >= '0' && input[cursor] <= '9') ++cursor;
        const size_t numberEnd = cursor;
        if (cursor < input.size() && input[cursor] == ':') {
            ++cursor;
            while (cursor < input.size() && input[cursor] != '}' &&
                   input[cursor] != '{' && input[cursor] != '\n' && input[cursor] != '\r') ++cursor;
        }
        unsigned n = 0;
        const auto digits = std::string_view(input).substr(start, numberEnd - start);
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), n);
        if (numberEnd > start && cursor < input.size() && input[cursor] == '}' &&
            parsed.ec == std::errc{} && n <= 100000 - offset) {
            output += '{';
            if (caret) output += '^';
            output += std::to_string(n + offset);
            output.append(input, numberEnd, cursor + 1 - numberEnd);
            i = cursor + 1;
        } else {
            output += input[i++];
        }
    }
    return output;
}

inline std::map<std::string, std::string> resolveLocaleReferences(
    std::map<std::string, std::string> values, const GameTextLookup& game) {
    std::set<std::string> processing;
    std::set<std::string> done;
    std::map<std::string, bool> gameExists;
    std::map<std::string, std::string> gameValues;

    const auto nativeExists = [&](const std::string& key) {
        if (values.contains(key)) return true;
        auto cached = gameExists.find(key);
        if (cached != gameExists.end()) return cached->second;
        std::string value;
        const bool found = game && game(key, &value);
        gameExists.emplace(key, found);
        if (found) gameValues.emplace(key, std::move(value));
        return found;
    };

    std::function<std::string(const std::string&, unsigned)> expand;
    expand = [&](const std::string& key, unsigned depth) -> std::string {
        auto found = values.find(key);
        if (found == values.end()) {
            if (nativeExists(key)) return gameValues.at(key);
            return key;
        }
        if (done.contains(key)) return found->second;
        if (depth >= 32 || !processing.insert(key).second) return key;
        const std::string source = found->second;
        std::string output;
        output.reserve(source.size());
        size_t last = 0;
        while (last < source.size()) {
            const size_t marker = source.find("{$", last);
            if (marker == std::string::npos) {
                output.append(source, last, source.size() - last);
                break;
            }
            output.append(source, last, marker - last);
            std::string reference;
            size_t after = marker;
            unsigned shift = 0;
            if (!parseLocaleReference(source, marker, &after, &reference, &shift)) {
                output.append(source, marker, 2);
                last = marker + 2;
                continue;
            }
            std::string resolved = reference;
            if (!nativeExists(resolved)) {
                size_t end = key.size();
                while (end > 0) {
                    const size_t dot = key.rfind('.', end - 1);
                    if (dot == std::string::npos) break;
                    const std::string candidate = key.substr(0, dot + 1) + reference;
                    if (nativeExists(candidate)) {
                        resolved = candidate;
                        break;
                    }
                    end = dot;
                }
            }
            output += shiftLocaleArguments(expand(resolved, depth + 1), shift);
            last = after;
        }
        processing.erase(key);
        done.insert(key);
        found->second = std::move(output);
        return found->second;
    };
    for (auto it = values.begin(); it != values.end(); ++it)
        expand(it->first, 0);
    return values;
}
}
