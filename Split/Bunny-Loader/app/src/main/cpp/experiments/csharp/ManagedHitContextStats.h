#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bl::csharp {

struct ManagedHitContextStats {
    int v2Calls = 0;
    int v2Successes = 0;
    int v2Failures = 0;
    int legacyCalls = 0;
    int legacyFailures = 0;
    int lastNpcIndex = -1;
    int minNpcIndex = -1;
    int maxNpcIndex = -1;
    int uniqueNpcIndices = 0;
    int invalidV2Indices = 0;
    std::array<uint64_t, 4> seenNpcIndices{};

    void record(bool usedV2, int npcIndex, bool success) {
        if (!usedV2) {
            ++legacyCalls;
            if (!success) ++legacyFailures;
            return;
        }
        ++v2Calls;
        if (success) ++v2Successes;
        else ++v2Failures;
        if (npcIndex < 0 || npcIndex >= 200) {
            ++invalidV2Indices;
            return;
        }
        lastNpcIndex = npcIndex;
        if (minNpcIndex < 0 || npcIndex < minNpcIndex) minNpcIndex = npcIndex;
        if (maxNpcIndex < 0 || npcIndex > maxNpcIndex) maxNpcIndex = npcIndex;
        uint64_t& word = seenNpcIndices[static_cast<size_t>(npcIndex / 64)];
        const uint64_t mask = uint64_t{1} << (npcIndex % 64);
        if (!(word & mask)) {
            word |= mask;
            ++uniqueNpcIndices;
        }
    }
};

}
