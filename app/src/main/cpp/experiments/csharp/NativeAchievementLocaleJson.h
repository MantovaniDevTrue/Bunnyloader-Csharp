#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace bl::csharp {

struct AchievementTranslation {
    std::string title;
    std::string description;
};

class AchievementLocaleParser {
public:
    explicit AchievementLocaleParser(std::string_view input) : source(input) {}

    bool parse(std::map<std::string, AchievementTranslation>* result) {
        if (!result || source.size() > 512 * 1024) return false;
        result->clear();
        if (!consume('{')) return false;
        if (!parsePairs([&](const std::string& key) {
            if (key == "Achievements") return parseEntries(result);
            return skipValue(0);
        })) return false;
        skipSpace();
        return index == source.size();
    }

private:
    std::string_view source;
    std::size_t index = 0;

    void skipSpace() {
        while (index < source.size() &&
            (source[index] == ' ' || source[index] == '\n' ||
             source[index] == '\r' || source[index] == '\t')) ++index;
    }
    bool consume(char expected) {
        skipSpace();
        if (index >= source.size() || source[index] != expected) return false;
        ++index;
        return true;
    }
    static void utf8(std::string* text, std::uint32_t c) {
        if (c <= 0x7F) text->push_back(static_cast<char>(c));
        else if (c <= 0x7FF) {
            text->push_back(static_cast<char>(0xC0 | (c >> 6)));
            text->push_back(static_cast<char>(0x80 | (c & 63)));
        } else if (c <= 0xFFFF) {
            text->push_back(static_cast<char>(0xE0 | (c >> 12)));
            text->push_back(static_cast<char>(0x80 | ((c >> 6) & 63)));
            text->push_back(static_cast<char>(0x80 | (c & 63)));
        } else {
            text->push_back(static_cast<char>(0xF0 | (c >> 18)));
            text->push_back(static_cast<char>(0x80 | ((c >> 12) & 63)));
            text->push_back(static_cast<char>(0x80 | ((c >> 6) & 63)));
            text->push_back(static_cast<char>(0x80 | (c & 63)));
        }
    }
    bool hex4(std::uint32_t* output) {
        if (index + 4 > source.size()) return false;
        *output = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = source[index++];
            const int n = (ch >= '0' && ch <= '9') ? ch - '0' :
                (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 :
                (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
            if (n < 0) return false;
            *output = (*output << 4) | static_cast<std::uint32_t>(n);
        }
        return true;
    }
    bool string(std::string* out) {
        if (!consume('"')) return false;
        out->clear();
        while (index < source.size()) {
            unsigned char ch = static_cast<unsigned char>(source[index++]);
            if (ch == '"') return true;
            if (ch < 32) return false;
            if (ch != '\\') {
                out->push_back(static_cast<char>(ch));
            } else {
                if (index >= source.size()) return false;
                const char e = source[index++];
                switch (e) {
                case '"': case '\\': case '/': out->push_back(e); break;
                case 'n': out->push_back('\n'); break;
                case 'r': out->push_back('\r'); break;
                case 't': out->push_back('\t'); break;
                case 'b': out->push_back('\b'); break;
                case 'f': out->push_back('\f'); break;
                case 'u': {
                    std::uint32_t u = 0;
                    if (!hex4(&u)) return false;
                    if (u >= 0xD800 && u <= 0xDBFF) {
                        if (index + 2 > source.size() ||
                            source[index++] != '\\' || source[index++] != 'u')
                            return false;
                        std::uint32_t low = 0;
                        if (!hex4(&low) || low < 0xDC00 || low > 0xDFFF)
                            return false;
                        u = 0x10000 + ((u - 0xD800) << 10) + low - 0xDC00;
                    } else if (u >= 0xDC00 && u <= 0xDFFF) return false;
                    utf8(out, u);
                    break;
                }
                default: return false;
                }
            }
            if (out->size() > 8192) return false;
        }
        return false;
    }
    template<class Fn>
    bool parsePairs(Fn onValue) {
        skipSpace();
        if (consume('}')) return true;
        for (;;) {
            std::string key;
            if (!string(&key) || !consume(':') || !onValue(key))
                return false;
            if (consume('}')) return true;
            if (!consume(',')) return false;
        }
    }
    bool skipValue(int depth) {
        if (depth > 40) return false;
        skipSpace();
        if (index >= source.size()) return false;
        if (source[index] == '{') {
            ++index;
            return parsePairs([&](const std::string&) {
                return skipValue(depth + 1);
            });
        }
        if (source[index] == '[') {
            ++index;
            if (consume(']')) return true;
            for (;;) {
                if (!skipValue(depth + 1)) return false;
                if (consume(']')) return true;
                if (!consume(',')) return false;
            }
        }
        if (source[index] == '"') {
            std::string unused;
            return string(&unused);
        }
        auto starts = [&](std::string_view literal) {
            if (source.substr(index).starts_with(literal)) {
                index += literal.size();
                return true;
            }
            return false;
        };
        if (starts("true") || starts("false") || starts("null")) return true;
        std::size_t first = index;
        if (source[index] == '-') ++index;
        if (index >= source.size()) return false;
        if (source[index] == '0') ++index;
        else {
            if (source[index] < '1' || source[index] > '9') return false;
            while (index < source.size() && std::isdigit(
                       static_cast<unsigned char>(source[index]))) ++index;
        }
        if (index < source.size() && source[index] == '.') {
            ++index;
            std::size_t fractional = index;
            while (index < source.size() && std::isdigit(
                       static_cast<unsigned char>(source[index]))) ++index;
            if (index == fractional) return false;
        }
        if (index < source.size() && (source[index] == 'e' || source[index] == 'E')) {
            ++index;
            if (index < source.size() &&
                (source[index] == '+' || source[index] == '-')) ++index;
            std::size_t exponent = index;
            while (index < source.size() && std::isdigit(
                       static_cast<unsigned char>(source[index]))) ++index;
            if (index == exponent) return false;
        }
        return index > first;
    }
    bool parseEntries(std::map<std::string, AchievementTranslation>* out) {
        if (!consume('{')) return false;
        return parsePairs([&](const std::string& name) {
            if (name.empty() || name.size() > 128 || out->size() >= 256)
                return false;
            if (!consume('{')) return false;
            AchievementTranslation value;
            std::string alternative;
            if (!parsePairs([&](const std::string& key) {
                if (key != "FriendlyName" && key != "Name" &&
                    key != "Description") return skipValue(0);
                std::string text;
                if (!string(&text) || text.size() > 4096) return false;
                if (key == "FriendlyName") value.title = std::move(text);
                else if (key == "Name") alternative = std::move(text);
                else value.description = std::move(text);
                return true;
            })) return false;
            if (value.title.empty()) value.title = std::move(alternative);
            if (!out->emplace(name, std::move(value)).second) return false;
            return true;
        });
    }
};

inline bool parseAchievementTranslations(
    std::string_view json, std::map<std::string, AchievementTranslation>* result) {
    return AchievementLocaleParser(json).parse(result);
}

}
