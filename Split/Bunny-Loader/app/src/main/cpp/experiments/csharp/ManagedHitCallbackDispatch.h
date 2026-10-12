#pragma once

namespace bl::csharp {

struct ManagedHitCallbackChoice {
    void* method = nullptr;
    bool usesV2 = false;
    bool usesV3 = false;
    bool usesV4 = false;
};

inline ManagedHitCallbackChoice chooseManagedHitCallback(
    void* legacy, void* v2, int npcIndex,
    void* v3 = nullptr, bool snapshotValid = false,
    void* v4 = nullptr, bool playerSnapshotValid = false) {
    if (npcIndex >= 0 && npcIndex < 200 && v4 &&
        snapshotValid && playerSnapshotValid)
        return {v4, false, false, true};
    if (npcIndex >= 0 && npcIndex < 200 && v3 && snapshotValid)
        return {v3, false, true, false};
    if (npcIndex >= 0 && npcIndex < 200 && v2)
        return {v2, true, false, false};
    return {legacy, false, false, false};
}

}
