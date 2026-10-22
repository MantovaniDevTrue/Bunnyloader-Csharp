#include "experiments/csharp/NativeDamageTypeRegistry.h"

#include <cassert>
#include <thread>
#include <atomic>

int main() {
    using namespace bl::csharp;
    NativeDamageTypeRegistry registry;
    NativeDamageTypeBinding state;
    assert(registry.publish("mod-a", {
        {"mod-a", 900, false, "V4"}, {"mod-a", 800, true, "C:ModA.SwordDamage"}
    }));
    assert(registry.publish("mod-b", {
        {"mod-b", 901, false, "V5"}, {"mod-b", 801, true, "C:ModB.BoltDamage"}
    }));
    assert(registry.lookup(false, 900, &state) && state.damageClass == "V4");
    assert(registry.lookup(true, 800, &state) && state.mod == "mod-a");
    assert(!registry.lookup(false, 800, &state));
    assert(!registry.publish("mod-c", {{"mod-c", 900, false, "V6"}}));
    assert(!registry.publish("mod-a", {{"mod-a", 902, false, "V1"}, {"other", 903, false, "V2"}}));
    assert(!registry.lookup(false, 902, &state));
    std::atomic<bool> valid{true};
    std::thread reader([&] {
        for (int i = 0; i < 12000; ++i)
            if (!registry.lookup(false, 901, &state) || state.damageClass != "V5")
                valid.store(false);
    });
    registry.unload("mod-a");
    reader.join();
    assert(valid.load());
    assert(!registry.lookup(false, 900, &state));
    assert(!registry.lookup(true, 800, &state));
    assert(!registry.publish("mod-new", {{"mod-new", 900, false, "V2"}}));
    assert(registry.publish("mod-new", {{"mod-new", 902, false, "V2"}}));
    assert(registry.lookup(false, 902, &state));
    assert(registry.lookup(false, 901, &state) && state.mod == "mod-b");
    assert(registry.lookup(true, 801, &state));
    registry.reset();
    assert(!registry.lookup(false, 901, &state));
}
