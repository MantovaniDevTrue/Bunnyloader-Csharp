#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    assert(sourceIdentityForBase("DamageClass", "Custom") == "DAMAGE_CLASS:Custom");
    assert(!sourceRequiresNativeRegistration("DamageClass"));

    std::vector<ParsedSourceType> types;
    std::string error;
    const std::string standalone = R"(
using BunnyLoader.Source;
namespace MyCombat;
public sealed class CustomDamage : DamageClass {
    public override StatInheritanceData GetModifierInheritance(DamageClass other) {
        return other == Generic ? StatInheritanceData.Full : StatInheritanceData.None;
    }
})";
    assert(parseCSharpSource(standalone, &types, &error));
    assert(types.size() == 1);
    assert(types[0].name == "CustomDamage");
    assert(types[0].base == "DamageClass");
    assert(types[0].namespaceName == "MyCombat");

    types.clear();
    error.clear();
    const std::string mixed = R"(
using BunnyLoader.Source;
namespace MixedCombat;
public sealed class CustomDamage : DamageClass { }
public sealed class SampleItem : BunnyItem {
    public override void SetDefaults() {
        DisplayName = "Sword";
        Damage = 20;
    }
})";
    assert(parseCSharpSource(mixed, &types, &error));
    assert(types.size() == 2);
    assert(types[0].base == "DamageClass");
    assert(types[1].base == "BunnyItem");
}
