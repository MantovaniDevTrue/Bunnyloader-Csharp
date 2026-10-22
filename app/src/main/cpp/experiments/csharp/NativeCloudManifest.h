#pragma once

#include "experiments/csharp/NativeCloudRegistry.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bl::csharp {

// Strict format emitted by CompiledAssemblyProbe.WriteCloudMetadata.
// All-or-nothing: if the manifest is corrupted, keep the existing registry
// unchanged. Only the owner mod can use a class name for its own clouds.
inline bool appendNativeCloudManifest(NativeCloudRegistry* registry,
                                      std::string_view ownerMod,
                                      std::string_view manifest,
                                      std::string* error) {
    if (error) error->clear();
    auto reject = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    if (!registry || registry->firstType() <= 0 ||
        ownerMod.empty() || ownerMod.size() > 72 ||
        manifest.empty() || manifest.size() > 65536 ||
        manifest.find('\0') != std::string_view::npos)
        return reject("CloudManifest: invalid owner, registry or input size");

    std::istringstream reader{std::string(manifest)};
    std::string line;
    if (!std::getline(reader, line))
        return reject("CloudManifest: missing version");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "BUNNY_CLOUDS_V1")
        return reject("CloudManifest: unsupported version");

    NativeCloudRegistry staged = *registry;
    std::size_t count = 0;
    while (std::getline(reader, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty())
            return reject("CloudManifest: empty content line");
        // The protocol has exactly four tab-separated fields, with no
        // escaping or arbitrary code executed by the native loader.
        const std::size_t p1 = line.find('\t');
        const std::size_t p2 = p1 == std::string::npos
            ? p1 : line.find('\t', p1 + 1);
        const std::size_t p3 = p2 == std::string::npos
            ? p2 : line.find('\t', p2 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos ||
            p3 == std::string::npos ||
            line.find('\t', p3 + 1) != std::string::npos ||
            line.substr(0, p1) != "CLOUD")
            return reject("CloudManifest: malformed content row");

        const std::string name = line.substr(p1 + 1, p2 - p1 - 1);
        const std::string texture = line.substr(p2 + 1, p3 - p2 - 1);
        const std::string rare = line.substr(p3 + 1);
        if ((rare != "0" && rare != "1") ||
            staged.add(ownerMod, name, texture, rare == "1") < 0)
            return reject("CloudManifest: duplicate, unsafe or overflowing cloud");
        ++count;
    }
    if (count == 0) return reject("CloudManifest: no cloud entries");
    *registry = std::move(staged);
    return true;
}

} // namespace bl::csharp
