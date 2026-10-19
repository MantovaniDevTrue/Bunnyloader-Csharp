#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceLimits.h"

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

bool contentDeclaration(const std::vector<Token>& tokens, size_t first, size_t last,
                        std::string* base, std::map<std::string, std::string>* properties,
                        std::string* error,std::vector<std::pair<std::string,std::string>>* localizedNames) {
    const std::string kinds = "|Item|Tile|Weapon|Projectile|Buff|BuffPotion|Accessory|Player|NPC|Prefix|";
    for (size_t i = first; i < last; ++i) {
        if(!tokens[i].literal&&(tokens[i].value=="BunnyName"||tokens[i].value=="BunnyNameAttribute")) {
            if(i+6>=last||tokens[i+1].value!="("||!tokens[i+2].literal||tokens[i+3].value!=","||!tokens[i+4].literal||tokens[i+5].value!=")"||tokens[i+6].value!="]") {
                *error="BunnyName exige cultura e texto literais";return false;
            }
            const auto& culture=tokens[i+2].value;const auto& text=tokens[i+4].value;
            bool valid=culture.size()>=2&&culture.size()<=20&&!text.empty()&&text.size()<=128&&localizedNames->size()<8;
            for(unsigned char c:culture)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))valid=false;
            for(unsigned char c:text)if(c<32||c==127)valid=false;
            for(const auto& name:*localizedNames)if(name.first==culture)valid=false;
            if(!valid){*error="BunnyName exige cultura unica, nome UTF-8 ate 128 bytes e no maximo 8 idiomas";return false;}
            localizedNames->push_back({culture,text});i+=6;continue;
        }
        if (tokens[i].literal || (tokens[i].value != "BunnyContent" &&
            tokens[i].value != "BunnyContentAttribute")) continue;
        if (i + 10 >= last || tokens[i + 1].value != "(" ||
            tokens[i + 2].value != "BunnyContentKind" || tokens[i + 3].value != "." ||
            kinds.find("|" + tokens[i + 4].value + "|") == std::string::npos ||
            tokens[i + 5].value != "," || !tokens[i + 6].literal ||
            tokens[i + 7].value != "," || !tokens[i + 8].literal ||
            tokens[i + 9].value != ")" || tokens[i + 10].value != "]" || !base->empty()) {
            *error = "BunnyContent exige categoria, nome e textura literais e uma unica declaracao";
            return false;
        }
        *base = "Bunny" + tokens[i + 4].value;
        if (*base != "BunnyPlayer") {
            properties->emplace("DisplayName", tokens[i + 6].value);
            if (*base != "BunnyPrefix") properties->emplace("Texture", tokens[i + 8].value);
        }
        i += 10;
    }
    return true;
}

bool statements(const std::vector<Token>& ts, size_t first, size_t last,
                ParsedSourceType* def, std::string* error) {
    for (size_t i = first; i < last;) {
        // These optional descriptors are executed by Mono; legacy metadata
        // extraction must not try to interpret constructor/fluent-call syntax.
        if(ts[i].value=="NativeDefaults"||ts[i].value=="Holdout") {
            size_t end=i;int parentheses=0;
            for(;end<last;++end){if(ts[end].value=="(")++parentheses;if(ts[end].value==")")--parentheses;if(ts[end].value==";"&&parentheses==0)break;}
            if(end==last){*error="descritor C# sem termino";return false;}i=end+1;continue;
        }
        if (i + 5 < last && ts[i].value == "base" && ts[i + 1].value == "." &&
            ts[i + 2].value == "SetDefaults" && ts[i + 3].value == "(" &&
            ts[i + 4].value == ")" && ts[i + 5].value == ";") {
            *error = "base.SetDefaults exige BunnyItem/BunnyTile; outras categorias usam defaults literais completos";
            return false;
        }
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
        if (key == "FishingCatchRules" && value == "BunnyQuestCatchRules" &&
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
        auto existing = def->properties.find(key);
        if (existing != def->properties.end() && existing->second == value &&
            (key == "DisplayName" || key == "Texture")) continue;
        if (!def->properties.emplace(key, value).second) {
            *error = "propriedade repetida: " + key;
            return false;
        }
    }
    return true;
}

bool parseClass(const std::vector<Token>& tokens, size_t begin, size_t end,
                const std::string& name, const std::string& base,
                const std::string& namespaceName,
                std::vector<ParsedSourceType>* definitions, std::string* error,
                const std::map<std::string, std::string>& declaredProperties = {}) {
    if (base != "BunnyItem" && base != "BunnyProjectile" && base != "BunnyWeapon" &&
        base != "BunnyBuff" && base != "BunnyBuffPotion" &&
        base != "BunnyPlayer" && base != "BunnyAccessory" &&
        base != "BunnyNPC" && base != "BunnyPrefix" && base != "BunnyTile" &&
        base != "BunnyScreenEffect") return true;
    ParsedSourceType def;
    def.name = name;
    def.base = base;
    def.namespaceName = namespaceName;
    def.properties = declaredProperties;
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
                if (i > member && (tokens[i - 1].value == "SetDefaults" ||
                    (base == "BunnyTile" && tokens[i - 1].value == "SetStaticDefaults")) &&
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
                    if (base == "BunnyItem" || base == "BunnyTile" || base == "BunnyProjectile") {
                        // So recursos/nomes sao necessarios ANTES do Mono. Todos
                        // os defaults numericos e receitas sao executados em C#;
                        // nao adivinhar expressoes nem descartar a sua funcionalidade.
                        for (size_t j=parametersEnd+2;j<bodyEnd;++j) {
                            const auto& key=tokens[j].value;
                            if (key!="Texture" && key!="DisplayName") continue;
                            if (j+3>=bodyEnd || tokens[j+1].value!="=" ||
                                !tokens[j+2].literal || tokens[j+3].value!=";") {
                                *error=name+": "+key+" precisa de string literal para registro antecipado (linha "+std::to_string(tokens[j].line)+")";
                                return false;
                            }
                            auto existing = def.properties.find(key);
                            if (existing != def.properties.end() &&
                                existing->second == tokens[j + 2].value) { j += 3; continue; }
                            if(!def.properties.emplace(key,tokens[j+2].value).second) {
                                *error=name+": recurso/nome repetido: "+key;return false;
                            }
                            j+=3;
                        }
                    } else if (!statements(tokens, parametersEnd + 2, bodyEnd, &def, error))
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
    if (!definitions || !error || source.size() > kSourceMaxFileBytes) return false;
    std::vector<Token> tokens;
    if (!tokenize(source, &tokens, error)) return false;
    size_t found = 0;
    size_t declarationStart = 0;
    struct NamespaceBlock { size_t end; std::string name; };
    std::vector<NamespaceBlock> namespaces;
    std::string fileNamespace;
    for (size_t i = 0; i < tokens.size(); ++i) {
        while (!namespaces.empty() && i >= namespaces.back().end)
            namespaces.pop_back();
        if (tokens[i].value == "namespace") {
            size_t next = i + 1;
            std::string name;
            if (next < tokens.size() && !tokens[next].literal) {
                name = tokens[next++].value;
                while (next + 1 < tokens.size() && tokens[next].value == ".") {
                    name += "." + tokens[next + 1].value;
                    next += 2;
                }
            }
            if (name.empty() || next >= tokens.size()) {
                *error = "namespace C# incompleto";
                return false;
            }
            if (tokens[next].value == ";") {
                if (!namespaces.empty() || !fileNamespace.empty()) {
                    *error = "namespace C# de arquivo duplicado ou aninhado";
                    return false;
                }
                fileNamespace = name;
                i = next;
                declarationStart = next + 1;
                continue;
            }
            if (tokens[next].value == "{") {
                const size_t end = closing(tokens, next, "{", "}");
                if (end == tokens.size()) {
                    *error = "namespace C# sem fechamento";
                    return false;
                }
                namespaces.push_back({end, name});
                i = next;
                declarationStart = next + 1;
                continue;
            }
            *error = "declaracao de namespace C# invalida";
            return false;
        }
        if (tokens[i].literal || tokens[i].value != "class" || i + 1 >= tokens.size()) continue;
        bool abstract = false;
        for (size_t j = declarationStart; j < i; ++j)
            if (!tokens[j].literal && tokens[j].value == "abstract") abstract = true;
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
        std::string declaredBase;
        std::map<std::string, std::string> declaredProperties;
        std::vector<std::pair<std::string,std::string>> localizedNames;
        if (!contentDeclaration(tokens, declarationStart, i, &declaredBase,
                                &declaredProperties, error,&localizedNames)) return false;
        if (!declaredBase.empty()) {
            const std::string roots = "|BunnyItem|BunnyTile|BunnyWeapon|BunnyProjectile|BunnyBuff|BunnyBuffPotion|BunnyAccessory|BunnyPlayer|BunnyNPC|BunnyPrefix|";
            if (roots.find("|" + base + "|") != std::string::npos && base != declaredBase) {
                *error = name + ": categoria BunnyContent difere da classe base";
                return false;
            }
            base = declaredBase;
        }
        if (!abstract && (base == "BunnyItem" || base == "BunnyProjectile" || base == "BunnyWeapon" ||
            base == "BunnyBuff" || base == "BunnyBuffPotion" ||
            base == "BunnyPlayer" || base == "BunnyAccessory" ||
            base == "BunnyNPC" || base == "BunnyPrefix" ||
            base == "BunnyTile" || base == "BunnyScreenEffect")) {
            std::string qualifiedNamespace = fileNamespace;
            for (const auto& scope : namespaces) {
                if (!qualifiedNamespace.empty()) qualifiedNamespace += ".";
                qualifiedNamespace += scope.name;
            }
            if (!parseClass(tokens, brace, end, name, base,
                            qualifiedNamespace, definitions, error, declaredProperties)) return false;
            definitions->back().localizedNames=std::move(localizedNames);
            ++found;
        }
        i = end;
        declarationStart = end + 1;
    }
    if (found == 0 && requireContent) {
        *error = "nenhuma classe BunnyItem, BunnyTile ou outro conteudo Bunny encontrada";
        return false;
    }
    return true;
}

}


