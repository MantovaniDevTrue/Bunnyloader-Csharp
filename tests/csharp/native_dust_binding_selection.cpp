#include "experiments/csharp/ManagedDustBindingSelection.h"

#include <cassert>

using namespace bl::csharp;

int main() {
    SourceModStatus mod;
    mod.id = "example";
    mod.dusts = 3;
    mod.registeredKindTypes["DUST:Plain"] = 332;
    mod.registeredKindTypes["DUST:Animated"] = 5000;
    mod.registeredKindTypes["DUST:Colorful"] = 733;
    mod.registeredKindTypes["ITEM:Animated"] = 1002;
    mod.dustMasks["Plain"] = 0;
    mod.dustMasks["Animated"] = 5;
    mod.dustMasks["Colorful"] = 8;
    auto bindings = selectManagedDustBindings(mod);
    assert(bindings.size() == 2);
    assert(bindings[0].first == 5000 && bindings[0].second == "Animated");
    assert(bindings[1].first == 733 && bindings[1].second == "Colorful");
    mod.dustMasks["Animated"] = 0;
    mod.dustMasks["Colorful"] = 0;
    assert(selectManagedDustBindings(mod).empty());
    mod.dustMasks.clear();
    assert(selectManagedDustBindings(mod).empty());
    mod.dustMasks["Plain"] = 1;
    mod.dusts = 0;
    assert(selectManagedDustBindings(mod).empty());
}
