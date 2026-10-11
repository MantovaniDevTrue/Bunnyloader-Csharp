#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "experiments/csharp/ManagedLiveEventQueue.h"

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
    int v3Calls = 0;
    int v3Successes = 0;
    int v3Failures = 0;
    int lastNpcType = -1;
    int lastNpcLife = -1;
    float lastNpcX = 0.0f;
    float lastNpcY = 0.0f;
    std::array<uint64_t, 4> seenNpcIndices{};

    void recordV3(int npcIndex, const ManagedNpcSnapshot& npc, bool success) {
        ++v3Calls;
        if (success) ++v3Successes;
        else ++v3Failures;
        if (npcIndex < 0 || npcIndex >= 200 || !managedNpcSnapshotIsValid(npc)) {
            ++invalidV2Indices;
            return;
        }
        lastNpcType = npc.npcType;
        lastNpcLife = npc.npcLife;
        lastNpcX = npc.posX;
        lastNpcY = npc.posY;
        trackIndex(npcIndex);
    }

    void trackIndex(int npcIndex) {
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
        trackIndex(npcIndex);
    }
};

}
