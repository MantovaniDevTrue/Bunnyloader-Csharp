#include "experiments/csharp/NativeDamageClassFlags.h"
#include <cassert>
#include <string_view>

int main() {
    using namespace bl::csharp;
    for (std::string_view key : {"V2", "V3"}) {
        auto f = nativeDamageClassFlags(key);
        assert(f.resolved && f.melee && !f.ranged && !f.magic && !f.summon);
    }
    auto ranged = nativeDamageClassFlags("V4");
    assert(ranged.resolved && ranged.ranged && !ranged.melee && !ranged.magic && !ranged.summon);
    auto magic = nativeDamageClassFlags("V5");
    assert(magic.resolved && magic.magic && !magic.melee && !magic.ranged && !magic.summon);
    for (std::string_view key : {"V6", "V7"}) {
        auto f = nativeDamageClassFlags(key);
        assert(f.resolved && f.summon && !f.ranged && !f.melee && !f.magic);
    }
    for (std::string_view key : {"V0", "V1", "V8", "V9",
                                  "C:ExampleModCSharp.DamageClasses.ExampleDamageClass",
                                  "V10", ""}) {
        auto f = nativeDamageClassFlags(key);
        assert(!f.resolved);
        assert(!f.melee && !f.ranged && !f.magic && !f.summon);
    }
}
