#pragma once

#include "experiments/csharp/NativeDamageClassStats.h"

#include <cmath>

namespace bl::csharp {

struct NativeVanillaPlayerDamage {
    float melee = 1.f;
    float ranged = 1.f;
    float magic = 1.f;
    float summon = 1.f;
};

inline bool nativeVanillaPlayerDamageValid(const NativeVanillaPlayerDamage& stats) {
    for (float value : {stats.melee, stats.ranged, stats.magic, stats.summon})
        if (!std::isfinite(value) || value < 0.f || value > 1000.f)
            return false;
    return true;
}

inline bool projectNativeClassDamageDelta(
    const NativeDamageClassStatDefaults& cls,
    const NativeVanillaPlayerDamage& player,
    float* delta) {
    if (!delta || !nativeVanillaPlayerDamageValid(player)) return false;
    const float sources[] = {
        player.melee, player.ranged, player.magic, player.summon
    };
    float weighted = 0.f;
    for (int source = 0; source < 4; ++source) {
        const float inheritance = cls.inheritance[static_cast<size_t>(source + 1)].damage;
        if (!std::isfinite(inheritance)) return false;
        weighted += (sources[source] - 1.f) * inheritance;
    }
    if (!std::isfinite(weighted) || std::abs(weighted) > 100000.f) return false;
    *delta = weighted;
    return true;
}

}
