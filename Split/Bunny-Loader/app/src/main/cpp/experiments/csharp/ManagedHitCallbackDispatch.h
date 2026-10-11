#pragma once

namespace bl::csharp {

struct ManagedHitCallbackChoice {
    void* method = nullptr;
    bool usesV2 = false;
    bool usesV3 = false;
};

inline ManagedHitCallbackChoice chooseManagedHitCallback(
    void* legacy, void* v2, int npcIndex,
    void* v3 = nullptr, bool snapshotValid = false) {
    if (npcIndex >= 0 && npcIndex < 200 && v3 && snapshotValid)
        return {v3, false, true};
    if (npcIndex >= 0 && npcIndex < 200 && v2)
        return {v2, true, false};
    return {legacy, false, false};
}

}
