#include "experiments/csharp/NativeCloudManifest.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using bl::csharp::NativeCloudRegistry;
    using bl::csharp::appendNativeCloudManifest;
    NativeCloudRegistry clouds;
    assert(clouds.start(50));
    std::string error;

    assert(appendNativeCloudManifest(&clouds, "alpha",
        "BUNNY_CLOUDS_V1\nCLOUD\tExampleCloud\tContent/Clouds/ExampleCloud\t0\n"
        "CLOUD\tLetterCloud\tContent/Clouds/LetterCloud\t1\n", &error));
    assert(clouds.count() == 2);
    assert(clouds.find(50)->name == "ExampleCloud");
    assert(clouds.find(50)->rare == false);
    assert(clouds.find(51)->rare == true);

    // Different mods can reuse source class names but not native IDs.
    assert(appendNativeCloudManifest(&clouds, "beta",
        "BUNNY_CLOUDS_V1\nCLOUD\tExampleCloud\tContent/Clouds/ExampleCloud\t0\n", &error));
    assert(clouds.count() == 3);
    assert(clouds.find(52)->modId == "beta");

    const std::string base = "BUNNY_CLOUDS_V1\n";
    auto mustRejectWithoutMutation = [&](const std::string& value) {
        const auto count = clouds.count();
        assert(!appendNativeCloudManifest(&clouds, "alpha", value, &error));
        assert(!error.empty());
        assert(clouds.count() == count);
        assert(clouds.find(52)->modId == "beta");
    };
    mustRejectWithoutMutation("INVALID_HEADER\nCLOUD\tFoo\tContent/Clouds/Foo\t0\n");
    mustRejectWithoutMutation(base);
    mustRejectWithoutMutation(base + "CLOUD\tBad\tContent/Clouds/Bad\t2\n");
    mustRejectWithoutMutation(base + "CLOUD\tBad\t../../escape\t0\n");
    mustRejectWithoutMutation(base + "CLOUD\tBad\tContent/Clouds/Bad\t0\textra\n");
    mustRejectWithoutMutation(base + "ITEM\tFoo\tContent/Clouds/Foo\t0\n");
    mustRejectWithoutMutation(base + "\n");
    mustRejectWithoutMutation(base +
        "CLOUD\tFresh\tContent/Clouds/Fresh\t1\n"
        "CLOUD\tExampleCloud\tContent/Clouds/ExampleCloud\t0\n");
    mustRejectWithoutMutation(base +
        "CLOUD\tFresh\tContent/Clouds/Fresh\t1\n"
        "CLOUD\tFresh\tContent/Clouds/Fresh\t0\n");
    mustRejectWithoutMutation(base + "CLOUD\tFoo\tContent/Clouds/Foo\t0\nBAD\n");

    assert(appendNativeCloudManifest(&clouds, "gamma",
        "BUNNY_CLOUDS_V1\r\nCLOUD\tOtherCloud\tContent/Clouds/OtherCloud\t0\r\n",
        &error));
    assert(clouds.count() == 4);
    assert(error.empty());
    assert(clouds.find(53)->name == "OtherCloud");

    // Fail-close at capacity without mutating any previous registrations.
    NativeCloudRegistry bounded;
    assert(bounded.start(200));
    std::string batch = base;
    for (int i = 0; i < 129; ++i)
        batch += "CLOUD\tCloud" + std::to_string(i) +
            "\tContent/Clouds/Cloud" + std::to_string(i) + "\t0\n";
    assert(!appendNativeCloudManifest(&bounded, "alpha", batch, &error));
    assert(bounded.count() == 0);

    std::cout << "CloudLoader C#: transactional native Roslyn manifest OK\n";
}
