#pragma once

#include "experiments/csharp/AutoSourceCache.h"
#include "experiments/csharp/NativeCommandMailbox.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {

// Le os descritores verificados pelo Roslyn/Mono, sem criar objetos de jogo.
// SHA256 impede que o cache de uma DLL reutilize aliases de outra DLL.
inline bool parseNativeCommandManifest(const std::string& wire,
                                       const std::string& assemblyPath,
                                       const std::vector<std::string>& expectedClasses,
                                       std::vector<NativeCommandSpec>* specs,
                                       std::string* error) {
    auto fail = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!specs || wire.empty() || wire.size() > 96 * 1024 ||
        wire.back() != '\n' || expectedClasses.size() > 128)
        return fail("CommandManifestInvalidSize");
    specs->clear();

    std::istringstream input(wire);
    std::string line;
    if (!std::getline(input, line) || line != "BUNNY_COMMANDS_V1")
        return fail("CommandManifestHeader");
    if (!std::getline(input, line) || !line.starts_with("ASSEMBLY_SHA256\t"))
        return fail("CommandManifestMissingAssemblySha");
    const std::string sha = line.substr(16);
    if (!auto_cache::digest(sha) ||
        sha != auto_cache::fileHash(assemblyPath, kSourceMaxAssemblyBytes))
        return fail("CommandManifestAssemblyMismatch");
    if (!std::getline(input, line) || !line.starts_with("CLASSES\t"))
        return fail("CommandManifestMissingClasses");

    auto parseNumber = [](const std::string& text, unsigned* out) {
        if (text.empty() || text.size() > 6) return false;
        unsigned value = 0;
        for (char c : text) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + static_cast<unsigned>(c - '0');
            if (value > 100000) return false;
        }
        *out = value;
        return true;
    };
    unsigned count = 0;
    if (!parseNumber(line.substr(8), &count) || count != expectedClasses.size())
        return fail("CommandManifestClassCountMismatch");

    std::set<std::string> expected(expectedClasses.begin(), expectedClasses.end());
    if (expected.size() != expectedClasses.size())
        return fail("CommandManifestDuplicateSourceClasses");
    std::set<std::string> names;
    struct Command {
        unsigned routes = 0;
        unsigned aliases = 0;
    };
    std::map<std::string, Command> entries;
    while (std::getline(input, line)) {
        if (line.empty()) return fail("CommandManifestEmptyLine");
        std::istringstream parts(line);
        std::string kind, className, name, route, extra;
        if (!std::getline(parts, kind, '\t') ||
            !std::getline(parts, className, '\t') ||
            !std::getline(parts, name, '\t') ||
            !std::getline(parts, route, '\t') ||
            std::getline(parts, extra, '\t'))
            return fail("CommandManifestInvalidRecord");
        if (expected.find(className) == expected.end())
            return fail("CommandManifestUnexpectedClass");
        unsigned flags = 0;
        if (!parseNumber(route, &flags) || flags == 0 || (flags & ~7u))
            return fail("CommandManifestInvalidRoutes");
        if (kind == "CMD") {
            if (!entries.emplace(className, Command{flags, 0}).second)
                return fail("CommandManifestDuplicateCommandClass");
        } else if (kind == "ALIAS") {
            auto owner = entries.find(className);
            if (owner == entries.end() || owner->second.routes != flags ||
                ++owner->second.aliases > 16)
                return fail("CommandManifestInvalidAlias");
        } else return fail("CommandManifestUnknownRecord");

        // A validação final do nome/alias acontece atomicamente em registerMod.
        std::string lowered;
        lowered.reserve(name.size());
        for (unsigned char c : name) {
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
            lowered.push_back(static_cast<char>(c));
        }
        if (!names.insert(lowered).second)
            return fail("CommandManifestDuplicateName");
        specs->push_back({name, static_cast<uint8_t>(flags)});
        if (specs->size() > NativeCommandMailbox::MaxAliasesPerMod)
            return fail("CommandManifestTooManyNames");
    }
    if (entries.size() != count || specs->size() < count)
        return fail("CommandManifestMissingCommands");
    return true;
}

} // namespace bl::csharp
