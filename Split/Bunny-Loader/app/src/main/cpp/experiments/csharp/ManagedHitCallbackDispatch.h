#pragma once

namespace bl::csharp {

struct ManagedHitCallbackChoice {
    void* method = nullptr;
    bool usesV2 = false;
};

inline ManagedHitCallbackChoice chooseManagedHitCallback(
    void* legacy, void* v2, int npcIndex) {
    if (npcIndex >= 0 && npcIndex < 200 && v2)
        return {v2, true};
    return {legacy, false};
}

}
