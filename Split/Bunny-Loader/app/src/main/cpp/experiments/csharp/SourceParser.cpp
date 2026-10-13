#include "experiments/csharp/SourceParser.h"

#include <cctype>
#include <string>
#include <vector>

namespace bl::csharp {
namespace {

struct Token {
    std::string value;
    int line;
    bool literal;
};

bool tokenize(const std::string& source, std::vector<Token>* out, std::string* error) {
    int line = 1;
    for (size_t i = 0; i < source.size();) {
        const unsigned char c = source[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (std::isspace(c)) { ++i; continue; }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            i += 2;
            while (i < source.size() && source[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            i += 2;
            bool closed = false;
            while (i + 1 < source.size()) {
                if (source[i] == '\n') ++line;
                if (source[i] == '*' && source[i + 1] == '/') {
                    i += 2;
                    closed = true;
                    break;
                }
                ++i;
            }
            if (!closed) { *error = "comentario C# sem fechamento"; return false; }
            continue;
        }
        if (std::isalpha(c) || c == '_') {
            const size_t start = i++;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) ||
                                        source[i] == '_')) ++i;
            out->push_back({source.substr(start, i - start), line, false});
            continue;
        }
        if (std::isdigit(c)) {
            const size_t start = i++;
            if (c == '0' && i < source.size() && (source[i] == 'x' || source[i] == 'X')) {
                ++i;
                const size_t digits = i;
                while (i < source.size() && std::isxdigit(static_cast<unsigned char>(source[i]))) ++i;
                if (i == digits) {
                    *error = "literal hexadecimal C# invalido na linha " + std::to_string(line);
                    return false;
                }
            } else {
                while (i < source.size() && (std::isdigit(static_cast<unsigned char>(source[i])) ||
                        source[i] == '.' || source[i] == 'f' || source[i] == 'F')) ++i;
            }
            out->push_back({source.substr(start, i - start), line, false});
            continue;
        }
        if (c == '"') {
            const int at = line;
            ++i;
            std::string value;
            bool closed = false;
            while (i < source.size()) {
                const char ch = source[i++];
                if (ch == '"') { closed = true; break; }
                if (ch == '\n') { *error = "string C# multilinha nao suportada"; return false; }
                if (ch == '\\') {
                    if (i >= source.size()) break;
                    const char escaped = source[i++];
                    if (escaped != '"' && escaped != '\\' && escaped != 'n') {
                        *error = "escape C# nao suportado";
                        return false;
                    }
                    value += escaped == 'n' ? '\n' : escaped;
                } else value += ch;
            }
            if (!closed) { *error = "string C# nao terminada"; return false; }
            out->push_back({value, at, true});
            continue;
        }
        if (std::string("{}()[]=;:.,-*+/%<>!&|?~^").find(static_cast<char>(c)) != std::string::npos) {
            out->push_back({std::string(1, static_cast<char>(c)), line, false});
            ++i;
            continue;
        }
        *error = "token C# ainda nao suportado na linha " + std::to_string(line);
        return false;
    }
    return true;
}

size_t closing(const std::vector<Token>& tokens, size_t open, const std::string& begin,
               const std::string& end) {
    if (open >= tokens.size() || tokens[open].value != begin) return tokens.size();
    int level = 0;
    for (size_t i = open; i < tokens.size(); ++i) {
        if (tokens[i].value == begin) ++level;
        if (tokens[i].value == end && --level == 0) return i;
    }
    return tokens.size();
}

bool statements(const std::vector<Token>& ts, size_t first, size_t last,
                ParsedSourceType* def, std::string* error) {
    for (size_t i = first; i < last;) {
        const std::string key = ts[i++].value;
        if (i >= last || ts[i++].value != "=") {
            *error = "somente atribuicoes em SetDefaults sao suportadas (linha " +
                     std::to_string(ts[i - 1].line) + ")";
            return false;
        }
        bool negative = false;
        if (i < last && ts[i].value == "-") { negative = true; ++i; }
        if (i >= last) return false;
        std::string value;
        if (ts[i].value == "nameof" && i + 3 < last &&
            ts[i + 1].value == "(" && ts[i + 3].value == ")") {
            value = ts[i + 2].value;
            i += 4;
        } else {
            value = ts[i++].value;
        }
        if (key == "Category" && value == "PrefixCategory" &&
            i + 1 < last && ts[i].value == ".") {
            value = ts[i + 1].value;
            i += 2;
        }
        if (key == "VisualMode" && value == "BunnyAccessoryVisualMode" &&
            i + 1 < last && ts[i].value == ".") {
            value = ts[i + 1].value;
            i += 2;
        }
        if (negative) value = "-" + value;
        if (i >= last || ts[i++].value != ";") {
            *error = "expressao C# nao suportada em " + key;
            return false;
        }
        if (!def->properties.emplace(key, value).second) {
            *error = "propriedade repetida: " + key;
            return false;
        }
    }
    return true;
}

bool parseClass(const std::vector<Token>& tokens, size_t begin, size_t end,
                const std::string& name, const std::string& base,
                std::vector<ParsedSourceType>* definitions, std::string* error) {
    if (base != "BunnyProjectile" && base != "BunnyWeapon" &&
        base != "BunnyBuff" && base != "BunnyBuffPotion" &&
        base != "BunnyPlayer" && base != "BunnyAccessory" &&
        base != "BunnyNPC" && base != "BunnyPrefix") return true;
    ParsedSourceType def;
    def.name = name;
    def.base = base;
    bool foundDefaults = false;
    size_t member = begin + 1;
    while (member < end) {
        bool advanced = false;
        for (size_t i = member; i < end;) {
            const auto& value = tokens[i].value;
            if (value == "(") {
                const size_t parametersEnd = closing(tokens, i, "(", ")");
                if (parametersEnd >= end) {
                    *error = name + ": parametros de membro C# sem fechamento";
                    return false;
                }
                if (i > member && tokens[i - 1].value == "SetDefaults" &&
                    parametersEnd == i + 1 && parametersEnd + 1 < end &&
                    tokens[parametersEnd + 1].value == "{") {
                    if (foundDefaults) {
                        *error = name + ": SetDefaults duplicado";
                        return false;
                    }
                    const size_t bodyEnd = closing(tokens, parametersEnd + 1, "{", "}");
                    if (bodyEnd >= end) {
                        *error = name + ": corpo SetDefaults sem fechamento";
                        return false;
                    }
                    if (!statements(tokens, parametersEnd + 2, bodyEnd, &def, error))
                        return false;
                    foundDefaults = true;
                    member = bodyEnd + 1;
                    advanced = true;
                    break;
                }
                i = parametersEnd + 1;
                continue;
            }
            if (value == "[") {
                const size_t bracketEnd = closing(tokens, i, "[", "]");
                if (bracketEnd >= end) {
                    *error = name + ": atributo ou indice C# sem fechamento";
                    return false;
                }
                i = bracketEnd + 1;
                continue;
            }
            if (value == "{") {
                const size_t bodyEnd = closing(tokens, i, "{", "}");
                if (bodyEnd >= end) {
                    *error = name + ": membro C# sem fechamento";
                    return false;
                }
                size_t next = bodyEnd + 1;
                if (next < end && tokens[next].value == "=") {
                    ++next;
                    while (next < end && tokens[next].value != ";") {
                        if (tokens[next].value == "(" || tokens[next].value == "{" ||
                            tokens[next].value == "[") {
                            const std::string closer = tokens[next].value == "(" ? ")" :
                                tokens[next].value == "{" ? "}" : "]";
                            const size_t endOfValue = closing(tokens, next, tokens[next].value, closer);
                            if (endOfValue >= end) {
                                *error = name + ": inicializador C# sem fechamento";
                                return false;
                            }
                            next = endOfValue;
                        }
                        ++next;
                    }
                    if (next >= end) {
                        *error = name + ": inicializador C# sem terminador";
                        return false;
                    }
                    ++next;
                } else if (next < end && tokens[next].value == ";") {
                    ++next;
                }
                member = next;
                advanced = true;
                break;
            }
            if (value == ";") {
                member = i + 1;
                advanced = true;
                break;
            }
            ++i;
        }
        if (!advanced) {
            *error = name + ": declaracao de membro C# incompleta";
            return false;
        }
    }
    if (!foundDefaults) {
        *error = name + ": precisa de void SetDefaults() com atribuicoes literais";
        return false;
    }
    definitions->push_back(std::move(def));
    return true;
}

}

bool parseCSharpSource(const std::string& source,
                       std::vector<ParsedSourceType>* definitions,
                       std::string* error, bool requireContent) {
    if (!definitions || !error || source.size() > 256 * 1024) return false;
    std::vector<Token> tokens;
    if (!tokenize(source, &tokens, error)) return false;
    size_t found = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].value != "class" || i + 1 >= tokens.size()) continue;
        const std::string name = tokens[i + 1].value;
        size_t brace = i + 2;
        std::string base;
        while (brace < tokens.size() && tokens[brace].value != "{") {
            if (tokens[brace].value == ":" && brace + 1 < tokens.size()) {
                base = tokens[brace + 1].value;
                size_t next = brace + 2;
                while (next + 1 < tokens.size() && tokens[next].value == ".") {
                    base = tokens[next + 1].value;
                    next += 2;
                }
            }
            ++brace;
        }
        if (brace == tokens.size()) {
            *error = name + ": classe sem corpo";
            return false;
        }
        const size_t end = closing(tokens, brace, "{", "}");
        if (end == tokens.size()) {
            *error = name + ": chave de fechamento ausente";
            return false;
        }
        if (base == "BunnyProjectile" || base == "BunnyWeapon" ||
            base == "BunnyBuff" || base == "BunnyBuffPotion" ||
            base == "BunnyPlayer" || base == "BunnyAccessory" ||
            base == "BunnyNPC" || base == "BunnyPrefix") {
            if (!parseClass(tokens, brace, end, name, base, definitions, error)) return false;
            ++found;
        }
        i = end;
    }
    if (found == 0 && requireContent) {
        *error = "nenhuma classe BunnyProjectile, BunnyWeapon, BunnyBuff, BunnyBuffPotion, BunnyPlayer, BunnyAccessory , BunnyNPC ou BunnyPrefix encontrada";
        return false;
    }
    return true;
}

}

