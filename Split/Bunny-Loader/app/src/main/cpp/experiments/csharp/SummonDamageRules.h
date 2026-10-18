#pragma once

namespace bl::csharp {
struct NativeSummonDamageDecision {
    int damage;
    bool recovered;
};

inline constexpr NativeSummonDamageDecision resolveNativeSummonDamage(
    int current, int configured) noexcept {
    if (current > 0 || configured < 1 || configured > 1000)
        return {current, false};
    return {configured, true};
}
}
