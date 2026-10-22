#include "experiments/csharp/NativeDamageCombatProbe.h"

#include <cassert>

int main() {
    using namespace bl::csharp;
    nativeDamageTypes().reset();
    nativeDamageClassStats().reset();
    const std::string wire =
        "DAMAGE_CLASS_STATS_V1\n"
        "CLASS\tExample.Damage\t0\t1.2\t1\t3\t0\t1\t1.5\t0\t8\t1.1\t10\t1\n";
    assert(nativeDamageClassStats().publish("combat-mod", wire));
    assert(nativeDamageTypes().publish("combat-mod", {
        {"combat-mod", 7000, false, "C:Example.Damage"},
        {"combat-mod", 7001, false, "V4"},
        {"combat-mod", 8000, true, "C:Example.Damage"}
    }));
    NativeDamageCombatSample sample;
    assert(!resolveNativeDamageCombatSample(-1, &sample));
    assert(!resolveNativeDamageCombatSample(7001, &sample));
    assert(!resolveNativeDamageCombatSample(8000, &sample));
    assert(resolveNativeDamageCombatSample(7000, &sample));
    assert(sample.source.mod == "combat-mod");
    assert(sample.defaults.damage.additive == 1.2f);
    assert(sample.defaults.damage.flat == 3.f);
    assert(sample.defaults.crit == 8.f);
    assert(sample.defaults.attackSpeed == 1.1f);
    assert(sample.defaults.armorPenetration == 10.f);
    nativeDamageClassStats().unload("combat-mod");
    assert(!resolveNativeDamageCombatSample(7000, &sample));
    assert(nativeDamageClassStats().publish("combat-mod",
        "DAMAGE_CLASS_STATS_V1\n"));
    assert(!resolveNativeDamageCombatSample(7000, &sample));
    nativeDamageTypes().unload("combat-mod");
    assert(!resolveNativeDamageCombatSample(7000, &sample));
    nativeDamageTypes().reset();
    nativeDamageClassStats().reset();
}
