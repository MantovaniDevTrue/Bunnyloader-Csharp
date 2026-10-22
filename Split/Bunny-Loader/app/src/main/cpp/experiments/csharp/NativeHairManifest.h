#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::hair {

// Contrato binário-independent entre o MonoVM e o registro C++.
// Campos TAB; nenhum campo admite TAB/LF. O registro é atômico:
// todo o lote passa ou nenhuma identidade é publicada.
inline bool ParseAndRegisterManifest(
    std::string_view wire, Registry* registry,
    const Registry::AssetExists& assetExists, std::string* error) {

    if (!registry || !error) return false;
    error->clear();
    constexpr std::string_view header = "BUNNY_HAIR_MANIFEST_V1\n";
    if (wire.size() < header.size() || wire.size() > 512 * 1024 ||
        !wire.starts_with(header)) {
        *error = "NativeHairManifestHeaderInvalid";
        return false;
    }
    wire.remove_prefix(header.size());
    std::vector<Declaration> proposed;
    int expectedType = registry->NextType();

    while (!wire.empty()) {
        const auto newline = wire.find('\n');
        if (newline == std::string_view::npos || newline == 0 || newline > 2048) {
            *error = "NativeHairManifestLineInvalid";
            return false;
        }
        const std::string_view line = wire.substr(0, newline);
        wire.remove_prefix(newline + 1);

        std::vector<std::string_view> parts;
        size_t begin = 0;
        for (size_t pos = 0; pos < line.size(); ++pos) {
            if (line[pos] == '\r' || line[pos] == '\0') {
                *error = "NativeHairManifestForbiddenCharacter";
                return false;
            }
            if (line[pos] == '\t') {
                parts.push_back(line.substr(begin, pos - begin));
                begin = pos + 1;
            }
        }
        parts.push_back(line.substr(begin));
        if (parts.size() != 8 || parts[0] != "HAIR") {
            *error = "NativeHairManifestRecordInvalid";
            return false;
        }

        const auto decode = [&](std::string_view value, int* out) {
            if (value.empty() || value.size() > 8) return false;
            int number = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != value.data() + value.size()) return false;
            *out = number;
            return true;
        };
        int type = -1, gender = -1, creation = -1;
        if (!decode(parts[1], &type) || type != expectedType ||
            !decode(parts[6], &gender) || gender < 0 || gender > 2 ||
            !decode(parts[7], &creation) || creation < 0 || creation > 1) {
            *error = "NativeHairManifestNumbersInvalid";
            return false;
        }
        if (!validIdentifier(parts[2]) || !validIdentifier(parts[3])) {
            *error = "NativeHairManifestIdentityInvalid";
            return false;
        }
        std::string mainPath, altPath, reason;
        if (!normalizeContentTexture(std::string(parts[4]).c_str(), &mainPath, &reason) ||
            !normalizeContentTexture(std::string(parts[5]).c_str(), &altPath, &reason) ||
            mainPath != parts[4] || altPath != parts[5]) {
            *error = "NativeHairManifestTextureInvalid";
            return false;
        }
        proposed.push_back({std::string(parts[2]), std::string(parts[3]),
                            std::move(mainPath), std::move(altPath),
                            static_cast<RandomGender>(gender), creation == 1});
        if (proposed.size() > 2048) {
            *error = "NativeHairManifestTooManyEntries";
            return false;
        }
        ++expectedType;
    }
    if (proposed.empty()) {
        *error = "NativeHairManifestEmpty";
        return false;
    }
    return registry->RegisterBatch(proposed, assetExists, error);
}

} // namespace bl::csharp::hair
