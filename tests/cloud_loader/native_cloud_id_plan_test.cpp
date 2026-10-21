#include "experiments/csharp/NativeCloudIdPlan.h"
#include "experiments/csharp/NativeCloudRegistry.h"

#include <cassert>
#include <iostream>

int main() {
    using bl::csharp::planNativeCloudIds;
    auto vanilla = planNativeCloudIds(40, 40, 40, 2, true, true);
    assert(vanilla.valid() && vanilla.firstType == 40 &&
           vanilla.lastExclusive == 42);

    // Two JS clouds have grown both arrays. Their IDs must be retained.
    auto coexist = planNativeCloudIds(40, 42, 42, 3, true, true);
    assert(coexist.valid() && coexist.firstType == 42 &&
           coexist.lastExclusive == 45);
    auto masksLarger = planNativeCloudIds(40, 42, 47, 2, true, true);
    assert(masksLarger.valid() && masksLarger.firstType == 47);
    auto texturesLarger = planNativeCloudIds(40, 47, 42, 2, true, true);
    assert(texturesLarger.valid() && texturesLarger.firstType == 47);

    // The JS CloudLoader has registered but may not have grown arrays
    // until Ready.Setup. No C# ID is safe during that intermediate stage.
    assert(!planNativeCloudIds(40, 40, 40, 2, false, true).valid());
    assert(!planNativeCloudIds(40, 40, 40, 2, true, false).valid());
    assert(!planNativeCloudIds(40, 39, 40, 2, true, true).valid());
    assert(!planNativeCloudIds(40, 40, 39, 2, true, true).valid());
    assert(!planNativeCloudIds(40, 40, 40, 0, true, true).valid());
    assert(!planNativeCloudIds(40, 40, 40, 129, true, true).valid());
    assert(!planNativeCloudIds(0, 40, 40, 2, true, true).valid());
    assert(!planNativeCloudIds(40, 10001, 40, 2, true, true).valid());
    assert(!planNativeCloudIds(40, 40, 10001, 2, true, true).valid());
    assert(!planNativeCloudIds(10001, 10001, 10001, 2, true, true).valid());

    bl::csharp::NativeCloudRegistry registry;
    assert(registry.start(coexist.firstType));
    assert(registry.add("csharpmod", "CloudOne",
        "Content/Clouds/CloudOne", false) == 42);
    assert(registry.add("csharpmod", "CloudTwo",
        "Content/Clouds/CloudTwo", true) == 43);
    assert(registry.find(40) == nullptr && registry.find(41) == nullptr);
    assert(registry.find(42)->modId == "csharpmod");

    std::cout << "CloudLoader C#: JS/CSharp ID ranges remain disjoint OK\n";
}
