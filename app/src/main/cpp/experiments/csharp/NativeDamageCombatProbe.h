#pragma once

#include "experiments/csharp/NativeDamageClassStats.h"
#include "experiments/csharp/NativeDamageTypeRegistry.h"

namespace bl::csharp {

struct NativeDamageCombatSample {
    NativeDamageTypeBinding source;
    NativeDamageClassStatDefaults defaults;
};

inline bool resolveNativeDamageCombatSample(int itemType, NativeDamageCombatSample* out) {
    if (!out || itemType < 0 || !nativeDamageTypes().isCustom(false, itemType))
        return false;
    NativeDamageTypeBinding binding;
    if (!nativeDamageTypes().lookup(false, itemType, &binding) ||
        !binding.damageClass.starts_with("C:")) return false;
    NativeDamageClassStatDefaults defaults;
    if (!nativeDamageClassStats().lookup(binding.mod, binding.damageClass, &defaults))
        return false;
    *out = {std::move(binding), defaults};
    return true;
}

}
