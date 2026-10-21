#pragma once

#include "experiments/csharp/NativeCloudRollCoordinator.h"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bl::csharp {

// Decoder for BunnyCloudDispatcher.WeightsWire. The worker may update each
// mod independently; every update is validated before touching the live pool.
// No allocations, string parsing or Mono calls occur inside addCloud.
inline bool appendNativeCloudWeightWire(
    const NativeCloudRegistry& registry, std::string_view modId,
    std::string_view wire, std::vector<NativeCloudWeight>* destination,
    std::string* error) {
    if (error) error->clear();
    auto reject = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!destination || modId.empty() || modId.size() > 72 ||
        wire.empty() || wire.size() > 8192 ||
        wire.find('\0') != std::string_view::npos)
        return reject("CloudWeights: invalid input");

    constexpr std::string_view header = "BUNNY_CLOUD_WEIGHTS_V1";
    const auto endHeader = wire.find('\n');
    if (endHeader == std::string_view::npos ||
        wire.substr(0, endHeader) != header)
        return reject("CloudWeights: unsupported version");

    std::vector<NativeCloudWeight> staged = *destination;
    std::size_t cursor = endHeader + 1;
    std::size_t processed = 0;
    while (cursor < wire.size()) {
        const auto end = wire.find('\n', cursor);
        if (end == std::string_view::npos)
            return reject("CloudWeights: truncated row");
        const auto line = wire.substr(cursor, end - cursor);
        const auto tab = line.find('\t');
        if (tab == std::string_view::npos ||
            line.find('\t', tab + 1) != std::string_view::npos ||
            tab == 0 || tab + 1 == line.size() ||
            line.size() > 72)
            return reject("CloudWeights: malformed row");
        int type = -1;
        const auto integer = line.substr(0, tab);
        auto parsed = std::from_chars(integer.data(),
            integer.data() + integer.size(), type);
        if (parsed.ec != std::errc() ||
            parsed.ptr != integer.data() + integer.size())
            return reject("CloudWeights: invalid cloud type");

        auto chanceString = std::string(line.substr(tab + 1));
        for (char ch : chanceString)
            if (!(ch >= '0' && ch <= '9') && ch != '.' && ch != '-' &&
                ch != '+' && ch != 'e' && ch != 'E')
                return reject("CloudWeights: invalid weight syntax");
        errno = 0;
        char* tail = nullptr;
        const double weight = std::strtod(chanceString.c_str(), &tail);
        if (errno || tail == chanceString.c_str() ||
            *tail != '\0' || !std::isfinite(weight) ||
            weight < 0 || weight > 1000000)
            return reject("CloudWeights: invalid weight number");

        const auto* entry = registry.find(type);
        if (!entry || entry->modId != modId ||
            staged.size() >= NativeCloudWeightedPool::kMaxModClouds)
            return reject("CloudWeights: unowned ID or capacity exceeded");
        for (const auto& prior : staged)
            if (prior.type == type)
                return reject("CloudWeights: repeated cloud ID");

        staged.push_back({type, weight});
        processed++;
        cursor = end + 1;
    }
    if (processed == 0)
        return reject("CloudWeights: no cloud weights");
    *destination = std::move(staged);
    return true;
}

} // namespace bl::csharp
