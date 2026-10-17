#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace bl::csharp;

    const std::vector<std::pair<std::string, std::string>> classes = {
        {"BunnyScreenEffect", "ProbeTint"},
        {"BunnyScreenEffect", "ProbeVignette"},
        {"BunnyItem", "ExampleItem"},
        {"BunnyPlayer", "ExamplePlayer"},
        {"BunnyProjectile", "ExampleProjectile"}
    };

    std::map<std::string, int> native;
    for (const auto& [base, name] : classes) {
        const std::string key = sourceIdentityForBase(base, name);
        assert(!key.empty());
        assert(native.emplace(key, -1).second);
    }
    native[sourceIdentityForBase("BunnyItem", "ExampleItem")] = 7001;
    native[sourceIdentityForBase("BunnyProjectile", "ExampleProjectile")] = 7002;

    for (const auto& [base, name] : classes)
        if (!sourceRequiresNativeRegistration(base))
            native.erase(sourceIdentityForBase(base, name));

    assert(native.size() == 2);
    assert(native.at("ITEM:ExampleItem") == 7001);
    assert(native.at("PROJECTILE:ExampleProjectile") == 7002);
    assert(!native.contains("SCREEN_EFFECT:ProbeTint"));
    assert(!native.contains("SCREEN_EFFECT:ProbeVignette"));
    assert(!native.contains("PLAYER:ExamplePlayer"));
    assert(sourceWireDomain("SCREEN_EFFECT").empty());
    assert(sourceIdentityForWire("ITEM", "ExampleItem") == "ITEM:ExampleItem");
    assert(sourceRequiresNativeRegistration("BunnyNPC"));
    assert(sourceRequiresNativeRegistration("BunnyTile"));
    assert(sourceRequiresNativeRegistration("BunnyAccessory"));
    assert(!sourceRequiresNativeRegistration("BunnyScreenEffect"));
    assert(!sourceRequiresNativeRegistration("BunnyPlayer"));

    const size_t managedDefaultsWireClasses = 3;
    const size_t playerClasses = 1;
    assert(managedDefaultsWireClasses == native.size() + playerClasses);
    return 0;
}
