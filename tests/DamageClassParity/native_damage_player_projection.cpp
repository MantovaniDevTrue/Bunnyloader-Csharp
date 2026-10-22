#include "experiments/csharp/NativeDamagePlayerProjection.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace bl::csharp;
    NativeDamageClassStatDefaults custom;
    for (auto& source : custom.inheritance)
        source = {0.f, 0.f, 0.f, 0.f, 0.f};
    custom.inheritance[0] = {1.f, 1.f, 1.f, 1.f, 1.f};

    float delta = -100.f;
    const NativeVanillaPlayerDamage equipped{1.4f, 1.2f, 1.3f, 1.5f};
    assert(projectNativeClassDamageDelta(custom, equipped, &delta));
    assert(std::abs(delta - .2f) < 0.0001f);

    custom.inheritance[1].damage = .5f;
    custom.inheritance[2].damage = -1.f;
    custom.inheritance[3].damage = 2.f;
    custom.inheritance[4].damage = 0.f;
    assert(projectNativeClassDamageDelta(custom, equipped, &delta));
    assert(std::abs(delta - .5f) < .0001f);
    assert(!projectNativeClassDamageDelta(custom, {NAN, 1.f, 1.f, 1.f}, &delta));
    assert(!projectNativeClassDamageDelta(custom, {1.f, -1.f, 1.f, 1.f}, &delta));
    assert(!projectNativeClassDamageDelta(custom, {1.f, 1.f, INFINITY, 1.f}, &delta));
    custom.inheritance[1].damage = 1.f;
    custom.inheritance[2].damage = 1.f;
    custom.inheritance[3].damage = 1.f;
    custom.inheritance[4].damage = 1.f;
    assert(projectNativeClassDamageDelta(custom, {1.25f, 1.25f, 1.25f, 1.25f}, &delta));
    assert(std::abs(delta - .25f) < .0001f);
    assert(projectNativeClassDamageDelta(custom, equipped, &delta));
    assert(std::abs(delta - .8f) < .0001f);
    custom.inheritance[1].damage = .5f;
    custom.inheritance[2].damage = -1.f;
    custom.inheritance[3].damage = 2.f;
    custom.inheritance[4].damage = 0.f;
    assert(!projectNativeClassDamageDelta(custom, equipped, nullptr));
    assert(std::abs(delta - .8f) < .0001f);
    assert(nativeVanillaPlayerDamageValid({1.f, 1.f, 1.f, 1.f}));
    assert(!nativeVanillaPlayerDamageValid({1001.f, 1.f, 1.f, 1.f}));

    custom.inheritance[1].crit = .5f;
    custom.inheritance[2].crit = -1.f;
    custom.inheritance[3].crit = 2.f;
    float sharedCrit = -100.f, critDelta = -100.f;
    assert(projectNativeClassCritDelta(custom, {20, 10, 15}, &sharedCrit, &critDelta));
    assert(sharedCrit == 10.f && std::abs(critDelta - 15.f) < .0001f);
    custom.inheritance[1].crit = 0.f;
    custom.inheritance[2].crit = 0.f;
    custom.inheritance[3].crit = 0.f;
    assert(projectNativeClassCritDelta(custom, {20, 10, 15}, &sharedCrit, &critDelta));
    assert(sharedCrit == 10.f && critDelta == 0.f);
    assert(!projectNativeClassCritDelta(custom, {1001, 0, 0}, &sharedCrit, &critDelta));
    assert(!projectNativeClassCritDelta(custom, {0, -1001, 0}, &sharedCrit, &critDelta));
    assert(!projectNativeClassCritDelta(custom, {0, 0, 0}, nullptr, &critDelta));
    assert(!projectNativeClassCritDelta(custom, {0, 0, 0}, &sharedCrit, nullptr));

    float speedDelta = -100.f;
    custom.inheritance[1].attackSpeed = 1.f;
    assert(projectNativeClassMeleeSpeedDelta(custom, 1.25f, &speedDelta));
    assert(std::abs(speedDelta - .25f) < .0001f);
    custom.inheritance[1].attackSpeed = -2.f;
    assert(projectNativeClassMeleeSpeedDelta(custom, 1.25f, &speedDelta));
    assert(std::abs(speedDelta + .5f) < .0001f);
    assert(!projectNativeClassMeleeSpeedDelta(custom, NAN, &speedDelta));
    assert(!projectNativeClassMeleeSpeedDelta(custom, -1.f, &speedDelta));
    assert(!projectNativeClassMeleeSpeedDelta(custom, 1.f, nullptr));
    assert(std::abs(speedDelta + .5f) < .0001f);

    float projectedPen = -100.f;
    custom.armorPenetration = 10.f;
    custom.inheritance[0].armorPenetration = 1.f;
    assert(projectNativeArmorPenetration(custom, 5, 3, &projectedPen));
    assert(projectedPen == 18.f);
    custom.inheritance[0].armorPenetration = 0.f;
    assert(projectNativeArmorPenetration(custom, 5, 3, &projectedPen));
    assert(projectedPen == 13.f);
    custom.inheritance[0].armorPenetration = -.5f;
    assert(projectNativeArmorPenetration(custom, 6, 3, &projectedPen));
    assert(projectedPen == 10.f);
    assert(!projectNativeArmorPenetration(custom, 100001, 1, &projectedPen));
    assert(!projectNativeArmorPenetration(custom, 0, -100001, &projectedPen));
    assert(!projectNativeArmorPenetration(custom, 1, 2, nullptr));
    assert(projectedPen == 10.f);
}
