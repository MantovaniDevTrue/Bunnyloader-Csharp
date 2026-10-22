#include "experiments/csharp/NativeCloudRollCoordinator.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace bl::csharp;
    NativeCloudRegistry registry;
    assert(registry.start(200));
    int common = registry.add("example", "Common", "Content/Clouds/Common", false);
    int rare = registry.add("example", "Rare", "Content/Clouds/Rare", true);
    int disabled = registry.add("example", "Disabled", "Content/Clouds/Disabled", false);
    assert(common == 200 && rare == 201 && disabled == 202);
    assert(registry.setInstalled(common, 203, 203, true, true, 80, 40, 0));
    assert(registry.setInstalled(rare, 203, 203, true, true, 96, 48, 0));
    NativeCloudRollCoordinator roll;
    assert(roll.refreshRequested());
    assert(roll.chooseCommon(.99) == 0);
    assert(roll.publish(registry, {{common, 1}, {rare, 10}, {disabled, 0}}));
    assert(roll.loaded() && !roll.refreshRequested());
    assert(roll.chooseCommon(22.5 / 23) == common);
    assert(roll.chooseRare(.65, false) == rare);
    assert(roll.chooseRare(.60, true) == rare);
    for (int n = 1; n < 60; ++n) roll.chooseCommon(.5);
    assert(roll.refreshRequested());

    // Invalid updates do not replace a previously validated snapshot.
    assert(!roll.publish(registry, {{common, NAN}}));
    assert(!roll.publish(registry, {{common, -1}}));
    assert(!roll.publish(registry, {{rare, INFINITY}}));
    assert(!roll.publish(registry, {{common, 2}, {common, 2}}));
    assert(!roll.publish(registry, {{203, 1}}));
    assert(!roll.publish(registry, {{disabled, 1}}));
    assert(roll.chooseCommon(.99) == common);

    assert(roll.publish(registry, {{rare, 0}, {common, 0}}));
    assert(roll.chooseCommon(.99) == 0);
    assert(roll.chooseRare(.99, true) == 0);
    roll.clear();
    assert(!roll.loaded() && roll.refreshRequested());
    assert(roll.chooseCommon(.99) == 0);
    std::cout << "CloudLoader C#: native common/rare cached MonoVM snapshot OK\n";
}
