#pragma once

#include "experiments/csharp/NativeDamageClassStats.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {

struct NativeVanillaPlayerDamage {
    float melee = 1.f;
    float ranged = 1.f;
    float magic = 1.f;
    float summon = 1.f;
};

struct NativeVanillaPlayerCrit {
    int32_t melee = 0;
    int32_t ranged = 0;
    int32_t magic = 0;
};

inline bool projectNativeClassCritDelta(
    const NativeDamageClassStatDefaults& cls,
    const NativeVanillaPlayerCrit& player,
    float* common, float* delta) {
    if (!common || !delta) return false;
    if (player.melee < -1000 || player.melee > 1000 ||
        player.ranged < -1000 || player.ranged > 1000 ||
        player.magic < -1000 || player.magic > 1000) return false;
    const int32_t base = std::min({player.melee, player.ranged, player.magic});
    const float sources[] = {
        static_cast<float>(player.melee - base),
        static_cast<float>(player.ranged - base),
        static_cast<float>(player.magic - base)
    };
    float result = 0.f;
    for (int i = 0; i < 3; ++i) {
        const float factor = cls.inheritance[static_cast<size_t>(i + 1)].crit;
        if (!std::isfinite(factor)) return false;
        result += sources[i] * factor;
    }
    if (!std::isfinite(result) || std::abs(result) > 100000.f) return false;
    *common = static_cast<float>(base);
    *delta = result;
    return true;
}

inline bool projectNativeClassMeleeSpeedDelta(
    const NativeDamageClassStatDefaults& cls,
    float meleeSpeed, float* delta) {
    if (!delta || !std::isfinite(meleeSpeed) || meleeSpeed < 0.f ||
        meleeSpeed > 1000.f) return false;
    const float factor = cls.inheritance[1].attackSpeed;
    if (!std::isfinite(factor)) return false;
    const float result = (meleeSpeed - 1.f) * factor;
    if (!std::isfinite(result) || std::abs(result) > 100000.f) return false;
    *delta = result;
    return true;
}

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
    const float common = std::min({sources[0], sources[1], sources[2], sources[3]});
    const float genericInheritance = cls.inheritance[0].damage;
    if (!std::isfinite(genericInheritance)) return false;
    float weighted = (common - 1.f) * genericInheritance;
    for (int source = 0; source < 4; ++source) {
        const float inheritance = cls.inheritance[static_cast<size_t>(source + 1)].damage;
        if (!std::isfinite(inheritance)) return false;
        weighted += (sources[source] - common) * inheritance;
    }
    if (!std::isfinite(weighted) || std::abs(weighted) > 100000.f) return false;
    *delta = weighted;
    return true;
}

}
