#include "experiments/csharp/ManagedAssemblyBindings.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <string>
#include <thread>

int main() {
    bl::csharp::ManagedAssemblyBindings bindings;
    assert(bindings.current("mod").empty());
    assert(!bindings.isCurrent("mod", "/tmp/mod.dll|bunny-revision=1"));
    bindings.bind("mod", "/tmp/mod.dll");
    const auto first = bindings.current("mod");
    assert(first == "/tmp/mod.dll|bunny-revision=1");
    assert(bindings.isCurrent("mod", first));
    bindings.bind("other", "/tmp/other.dll");
    const auto other = bindings.current("other");
    assert(other == "/tmp/other.dll|bunny-revision=1");
    bindings.bind("mod", "/tmp/mod.dll");
    const auto second = bindings.current("mod");
    assert(second == "/tmp/mod.dll|bunny-revision=2");
    assert(!bindings.isCurrent("mod", first));
    assert(bindings.isCurrent("mod", second));
    assert(bindings.isCurrent("other", other));
    bindings.unbind("mod");
    assert(bindings.current("mod").empty());
    assert(!bindings.isCurrent("mod", second));
    bindings.bind("mod", "/tmp/mod.dll");
    assert(bindings.current("mod") == "/tmp/mod.dll|bunny-revision=3");
    assert(!bindings.isCurrent("mod", first));
    assert(!bindings.isCurrent("mod", second));
    bindings.bind("", "/tmp/invalid.dll");
    bindings.bind("invalid", "");
    assert(bindings.current("").empty());
    assert(bindings.current("invalid").empty());

    std::atomic<bool> stopped{false};
    std::atomic<int> samples{0};
    std::thread reader([&] {
        while (!stopped.load(std::memory_order_acquire)) {
            const auto value = bindings.current("mod");
            if (!value.empty()) {
                assert(value.rfind("/tmp/mod.dll|bunny-revision=", 0) == 0);
                ++samples;
            }
            assert(bindings.isCurrent("other", other));
        }
    });
    for (int i = 0; i < 1000; ++i) {
        bindings.unbind("mod");
        assert(!bindings.isCurrent("mod", first));
        bindings.bind("mod", "/tmp/mod.dll");
    }
    stopped.store(true, std::memory_order_release);
    reader.join();
    assert(bindings.current("mod") == "/tmp/mod.dll|bunny-revision=1003");
    assert(!bindings.isCurrent("mod", first));
    assert(samples.load() >= 0);
    std::cout << "ANGLER_MANAGED_BINDINGS_LIFECYCLE_OK\n";
}
