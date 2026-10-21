#include "experiments/csharp/NativeCloudAutoload.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
        ("bunny-cloud-autoload-test-" + std::to_string(
            static_cast<unsigned long long>(
                std::hash<std::string>{}(fs::current_path().string()))));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "content/Content/Clouds", ec);
    assert(!ec);
    fs::create_directories(root / "content/Other/Clouds", ec);
    assert(!ec);
    fs::create_directories(root / "content/Assets/Textures/Clouds", ec);
    assert(!ec);
    auto touch = [&](const fs::path& path) {
        std::ofstream out(path, std::ios::binary);
        out << "PNG fixture";
        assert(out.good());
    };
    touch(root / "content/Content/Clouds/ExampleAdvancedCloud.png");
    touch(root / "content/Content/Clouds/ExampleCloud.png");
    touch(root / "content/Other/Clouds/Nested.png");
    touch(root / "content/Assets/Textures/Clouds/Legacy.png");
    touch(root / "content/Content/Clouds/Bad-name.png");
    touch(root / "content/Content/NotCloud.png");
    fs::create_symlink(root / "content/Content/Clouds/ExampleCloud.png",
        root / "content/Content/Clouds/Symlink.png", ec);
    assert(!ec);

    using namespace bl::csharp;
    NativeCloudRegistry registry;
    assert(registry.start(200));
    assert(registry.add("example", "ExampleAdvancedCloud",
        "Content/Clouds/ExampleAdvancedCloud", true) == 200);
    std::string error;
    assert(appendNativeCloudAutoload(&registry, "example", root.string(), &error));
    assert(registry.count() == 4);
    assert(registry.find(200)->rare);
    assert(registry.find(201)->name == "Legacy");
    assert(registry.find(202)->name == "ExampleCloud");
    assert(registry.find(203)->name == "Nested");
    for (int i = 201; i <= 203; ++i)
        assert(!registry.find(i)->rare);

    // Re-running autoload is deterministic and does not allocate duplicates.
    assert(appendNativeCloudAutoload(&registry, "example", root.string(), &error));
    assert(registry.count() == 4);

    auto before = registry.count();
    assert(!appendNativeCloudAutoload(&registry, "example",
        (root / "missing").string(), &error));
    assert(registry.count() == before);
    assert(!error.empty());

    fs::remove_all(root, ec);
    assert(!ec);
    std::cout << "CloudLoader C#: ExampleModBunny automatic Clouds PNG registration OK\n";
}
