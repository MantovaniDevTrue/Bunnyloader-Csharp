#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using namespace bl::csharp;
    assert(sourceIdentityForBase("BunnyDust", "Sparks") == "DUST:Sparks");
    assert(sourceIdentityForWire("DUST", "Sparks") == "DUST:Sparks");
    assert(sourceRequiresNativeRegistration("BunnyDust"));
    std::vector<ParsedSourceType> definitions;
    std::string error;

    const std::string source = R"(
using BunnyLoader.Source;
namespace TestMod;
public sealed class Sparks : BunnyDust {
    public override void SetStaticDefaults() {
        Texture = "Content/Dusts/Sparks.png";
        UpdateType = 6;
    }
    public override bool Update(ref BunnyDustState state) { return false; }
    public override bool PreDraw(ref BunnyDustState state) { return true; }
})";
    assert(parseCSharpSource(source, &definitions, &error));
    assert(definitions.size() == 1);
    assert(definitions[0].name == "Sparks");
    assert(definitions[0].base == "BunnyDust");
    assert(definitions[0].properties.at("Texture") == "Content/Dusts/Sparks.png");

    definitions.clear();
    const std::string inherited = R"(
using BunnyLoader.Source;
namespace TestMod;
[BunnyContent(BunnyContentKind.Dust, "Ember", "Content/Dusts/Ember.png")]
public sealed class Ember : CustomDustBase {
    public override void SetStaticDefaults() { }
})";
    assert(parseCSharpSource(inherited, &definitions, &error));
    assert(definitions.size() == 1);
    assert(definitions[0].base == "BunnyDust");
    assert(definitions[0].properties.at("Texture") == "Content/Dusts/Ember.png");

    definitions.clear();
    const std::string invalid = R"(
using BunnyLoader.Source;
public sealed class InvalidDust : BunnyDust {
    public override void SetStaticDefaults() {
        Texture = SomeDynamicValue;
    }
})";
    assert(!parseCSharpSource(invalid, &definitions, &error));
    assert(error.find("string literal") != std::string::npos);

    definitions.clear();
    const std::string missing = R"(
using BunnyLoader.Source;
public sealed class MissingDefaults : BunnyDust {
    public bool Update(ref BunnyDustState state) { return false; }
})";
    assert(!parseCSharpSource(missing, &definitions, &error));
}
