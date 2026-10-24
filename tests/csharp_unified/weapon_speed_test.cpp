#include "experiments/csharp/WeaponSpeedContract.h"
#include <cassert>
#include <limits>

int main() {
    using bl::csharp::validNativeWeaponShootSpeed;
    assert(validNativeWeaponShootSpeed(0.f, true));
    assert(!validNativeWeaponShootSpeed(0.f, false));
    assert(validNativeWeaponShootSpeed(1.f, false));
    assert(validNativeWeaponShootSpeed(40.f, false));
    assert(validNativeWeaponShootSpeed(40.f, true));
    assert(!validNativeWeaponShootSpeed(-0.01f, true));
    assert(!validNativeWeaponShootSpeed(40.01f, true));
    assert(!validNativeWeaponShootSpeed(std::numeric_limits<float>::infinity(), true));
    assert(!validNativeWeaponShootSpeed(std::numeric_limits<float>::quiet_NaN(), true));
    return 0;
}
