#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    std::string error;
    std::vector<ParsedSourceType> definitions;
    const std::string globalOnly = R"(
namespace PrefixFixture;
public sealed class TestGlobal : BunnyGlobalItem
{
    public override bool AppliesTo(BunnyPrefixItemSnapshot item) => item.Damage >= 10;
    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => prefix != 80;
    public override int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public override bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public override void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix,
                                     ref BunnyPrefixApplyStats effects) { }
}
)";
    if (!parseCSharpSource(globalOnly, &definitions, &error, false)) {
        std::cerr << error << '\n';
        return 1;
    }
    assert(definitions.size() == 1);
    assert(definitions[0].name == "TestGlobal");
    assert(definitions[0].base == "BunnyGlobalItem");
    assert(definitions[0].namespaceName == "PrefixFixture");
    assert(definitions[0].properties.empty());
    assert(sourceIdentityForBase("BunnyGlobalItem", "TestGlobal") ==
           "GLOBAL_PREFIX:TestGlobal");
    assert(!sourceRequiresNativeRegistration("BunnyGlobalItem"));
    assert(sourceRequiresNativeRegistration("BunnyWeapon"));
    definitions.clear();
    error.clear();
    const std::string mixed = R"(
namespace PrefixFixture;
public sealed class TestGlobal : BunnyGlobalItem
{
    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => false;
}
public sealed class TestWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        Damage = 12;
        Shoot = "TestBolt";
    }
}
)";
    if (!parseCSharpSource(mixed, &definitions, &error, false)) {
        std::cerr << error << '\n';
        return 2;
    }
    assert(definitions.size() == 2);
    assert(definitions[0].base == "BunnyGlobalItem");
    assert(definitions[1].base == "BunnyWeapon");
    assert(sourceIdentityForBase(definitions[0].base, definitions[0].name) !=
           sourceIdentityForBase(definitions[1].base, definitions[1].name));
    std::cout << "BUNNY_GLOBAL_PREFIX_SOURCE_PARSER=PASS" << std::endl;
}
