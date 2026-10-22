#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bl::csharp {

struct NativeBiomeDescriptor {
    std::string modId;
    std::string className;
    std::vector<std::string> tileNames;
};

inline bool validNativeBiomeIdentifier(std::string_view id, bool owner) {
    if (id.empty() || id.size() > 72) return false;
    for (char c : id)
        if (!(c >= 'a' && c <= 'z') &&
            !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') &&
            c != '_' && !(owner && c == '-'))
            return false;
    return true;
}

inline bool appendNativeBiomeManifest(
    std::string_view modId, std::string_view payload,
    std::vector<NativeBiomeDescriptor>* destination,
    std::string* error) {
    if (error) error->clear();
    auto reject = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!destination || !validNativeBiomeIdentifier(modId, true) ||
        payload.size() < 16 || payload.size() > 32768 ||
        payload.find('\0') != std::string_view::npos)
        return reject("BiomeManifest: invalid input");
    constexpr std::string_view version = "BUNNY_BIOMES_V1\n";
    if (!payload.starts_with(version))
        return reject("BiomeManifest: unsupported version");

    auto staged = *destination;
    std::size_t position = version.size(), added = 0;
    while (position < payload.size()) {
        const auto end = payload.find('\n', position);
        if (end == std::string_view::npos)
            return reject("BiomeManifest: truncated row");
        const auto row = payload.substr(position, end - position);
        const auto first = row.find('\t');
        const auto second = first == std::string_view::npos
            ? first : row.find('\t', first + 1);
        if (first != 5 || row.substr(0, first) != "BIOME" ||
            second == std::string_view::npos ||
            row.find('\t', second + 1) != std::string_view::npos)
            return reject("BiomeManifest: invalid row");
        std::string name(row.substr(first + 1, second - first - 1));
        if (!validNativeBiomeIdentifier(name, false))
            return reject("BiomeManifest: invalid class name");

        NativeBiomeDescriptor entry{std::string(modId), std::move(name), {}};
        const auto deps = row.substr(second + 1);
        size_t offset = 0;
        while (offset < deps.size()) {
            auto comma = deps.find(',', offset);
            if (comma == std::string_view::npos) comma = deps.size();
            const auto token = deps.substr(offset, comma - offset);
            if (!validNativeBiomeIdentifier(token, false) ||
                std::find(entry.tileNames.begin(), entry.tileNames.end(),
                          token) != entry.tileNames.end() ||
                entry.tileNames.size() >= 32)
                return reject("BiomeManifest: invalid tile dependency");
            entry.tileNames.emplace_back(token);
            offset = comma + 1;
            if (comma + 1 == deps.size())
                return reject("BiomeManifest: trailing comma");
        }
        for (const auto& prior : staged)
            if (prior.modId == entry.modId &&
                prior.className == entry.className)
                return reject("BiomeManifest: duplicate class");
        if (staged.size() >= 1024)
            return reject("BiomeManifest: capacity exceeded");
        staged.push_back(std::move(entry));
        ++added;
        position = end + 1;
    }
    if (added == 0 || added > 64)
        return reject("BiomeManifest: empty or oversized mod registry");
    *destination = std::move(staged);
    return true;
}

inline bool appendNativeBiomeActiveKeys(
    const std::vector<NativeBiomeDescriptor>& registry,
    std::string_view modId, std::string_view payload,
    bool gameMenu, std::vector<std::string>* output,
    std::string* error) {
    if (error) error->clear();
    auto reject = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    constexpr std::string_view version = "BUNNY_BIOMES_V1\n";
    if (!output || !validNativeBiomeIdentifier(modId, true) ||
        !payload.starts_with(version) ||
        payload.size() > 8192 || payload.find('\0') != std::string_view::npos)
        return reject("BiomeStatus: invalid input");
    auto staged = *output;
    std::size_t position = version.size();
    std::size_t processed = 0;
    while (position < payload.size()) {
        const auto end = payload.find('\n', position);
        if (end == std::string_view::npos)
            return reject("BiomeStatus: truncated row");
        const auto row = payload.substr(position, end - position);
        if (!row.starts_with("BIOME\t") ||
            row.find('\t', 6) != std::string_view::npos ||
            gameMenu || processed++ >= 64)
            return reject("BiomeStatus: invalid row or menu state");
        const auto name = row.substr(6);
        if (!validNativeBiomeIdentifier(name, false))
            return reject("BiomeStatus: invalid class");
        bool owned = false;
        for (const auto& entry : registry)
            if (entry.modId == modId && entry.className == name) {
                owned = true;
                break;
            }
        if (!owned)
            return reject("BiomeStatus: unregistered biome");
        std::string key(modId);
        key += '/';
        key.append(name);
        if (std::find(staged.begin(), staged.end(), key) != staged.end())
            return reject("BiomeStatus: duplicate active key");
        staged.push_back(std::move(key));
        position = end + 1;
    }
    *output = std::move(staged);
    return true;
}

} // namespace bl::csharp
