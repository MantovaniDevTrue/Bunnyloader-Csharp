#include "experiments/csharp/NativeCloudRegistry.h"

#include <cassert>
#include <iostream>

using bl::csharp::NativeCloudRegistry;
using bl::csharp::NativeCloudWeightedPool;

int main() {
    NativeCloudRegistry registry;
    assert(!registry.start(-1));
    assert(registry.start(64)); // The real base comes from CloudID.Count.
    assert(registry.add("ExampleModCSharp", "ExampleCloud",
        "Content/Clouds/ExampleCloud", false) == 64);
    assert(registry.add("ExampleModCSharp", "ExampleAdvancedCloud",
        "Content/Clouds/ExampleAdvancedCloud", true) == 65);
    assert(registry.add("OtherMod", "ExampleCloud", "Clouds/ExampleCloud",
        false) == 66);
    assert(registry.add("ExampleModCSharp", "ExampleCloud", "Clouds/Duplicate",
        false) == -1);
    assert(registry.add("4a30c8f0-97c9-4d38-a4a7-731c58e0d43d",
        "GuidCloud", "Content/Clouds/GuidCloud", false) == 67);
    assert(registry.add("Bad-Mod", "Bad-Cloud", "Clouds/C", false) == -1);
    assert(registry.add("Bad/Mod", "Cloud", "Clouds/C", false) == -1);
    assert(registry.add("ExampleModCSharp", "BadCloud", "../Bad", false) == -1);
    assert(registry.add("ExampleModCSharp", "BadCloud", "/absolute", false) == -1);
    assert(registry.count() == 4);
    assert(registry.find(63) == nullptr);
    assert(registry.find(68) == nullptr);

    NativeCloudWeightedPool common, rare;
    assert(common.reset(22));
    assert(rare.reset(18));
    // A missing mobile horizon mask must prevent a crash, not just warn.
    assert(!registry.setInstalled(64, 67, 64, true, false, 80, 24, 0));
    assert(!registry.addWeight(common, 64, false, 1));
    assert(!registry.setInstalled(65, 67, 67, true, true, 80, 24, 80));
    assert(!registry.addWeight(rare, 65, true, 10));
    assert(registry.setInstalled(64, 67, 67, true, true, 80, 24, 0));
    assert(registry.setInstalled(65, 67, 67, true, true, 96, 48, 0));
    assert(registry.addWeight(common, 64, false, 1));
    assert(!registry.addWeight(common, 65, false, 10));
    assert(registry.addWeight(rare, 65, true, 10));
    assert(common.choose(22.5 / 23) == 64);
    assert(rare.choose(0.65) == 65);
    // After a failed mask recheck an earlier ready cloud stops spawning.
    assert(!registry.setInstalled(65, 67, 65, true, true, 96, 48, 0));
    assert(rare.reset(18));
    assert(!registry.addWeight(rare, 65, true, 10));

    registry.clear();
    assert(registry.find(64) == nullptr);
    assert(registry.add("ExampleModCSharp", "Other", "Clouds/Other", false) == -1);
    std::cout << "CloudLoader C#: registry/source-only tests OK (no APK)\n";
}
