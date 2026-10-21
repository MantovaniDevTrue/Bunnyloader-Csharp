#include "experiments/csharp/NativeCloudWeightWire.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    NativeCloudRegistry registry;
    assert(registry.start(200));
    assert(registry.add("alpha", "Common", "Content/Clouds/Common", false) == 200);
    assert(registry.add("alpha", "Rare", "Content/Clouds/Rare", true) == 201);
    assert(registry.add("beta", "Cloud", "Content/Clouds/Cloud", false) == 202);
    assert(registry.setInstalled(200,203,203,true,true,32,24,0));
    assert(registry.setInstalled(201,203,203,true,true,64,24,0));
    assert(registry.setInstalled(202,203,203,true,true,32,16,0));

    std::vector<NativeCloudWeight> weights;
    std::string error;
    assert(appendNativeCloudWeightWire(registry, "alpha",
        "BUNNY_CLOUD_WEIGHTS_V1\n200\t1\n201\t10\n", &weights, &error));
    assert(weights.size() == 2 && weights[1].type == 201 &&
        weights[1].chance == 10);
    assert(appendNativeCloudWeightWire(registry, "beta",
        "BUNNY_CLOUD_WEIGHTS_V1\n202\t0\n", &weights, &error));
    assert(weights.size() == 3);

    auto reject = [&](std::string_view input, std::string_view mod = "beta") {
        const auto existing = weights.size();
        assert(!appendNativeCloudWeightWire(registry, mod,
            input, &weights, &error));
        assert(weights.size() == existing && !error.empty());
        assert(weights[0].type == 200 && weights[1].chance == 10);
    };
    reject("INVALID\n202\t1\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t1\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n200\t1\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\tNaN\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\tinf\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t-2\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t1e309\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t1,3\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t2\t3\n");
    reject("BUNNY_CLOUD_WEIGHTS_V1\n202\t2");

    NativeCloudRollCoordinator rolls;
    assert(rolls.publish(registry, weights));
    assert(rolls.chooseCommon(22.5 / 24.0) == 200);
    assert(rolls.chooseRare(.8, false) == 201);
    std::cout << "CloudLoader C#: managed weights ownership and native roll OK\n";
}
