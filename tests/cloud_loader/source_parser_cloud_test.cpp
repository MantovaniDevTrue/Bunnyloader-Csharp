#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    std::vector<bl::csharp::ParsedSourceType> parsed;
    std::string error;

    // The CSharp source parser must recognize the actual ExampleModBunny
    // cloud pattern rather than silently treating it as a helper class.
    const std::string source = R"CS(
using BunnyLoader.Source;
namespace ExampleModCSharp.CloudExamples;
public sealed class ExampleAdvancedCloud : BunnyCloud {
    public override bool RareCloud => true;
    public override void SetDefaults() {
        Texture = "Content/Clouds/ExampleAdvancedCloud";
    }
    public override float SpawnChance(BunnyCloudEnvironment context) =>
        context.InBiome("ExampleSurfaceBiome") ? 10f : 1f;
    public override void OnSpawn(BunnyCloudSpawn cloud) {
        cloud.SpriteEffects = BunnyCloudSpriteEffects.None;
    }
})CS";
    assert(bl::csharp::parseCSharpSource(source, &parsed, &error, true));
    assert(parsed.size() == 1);
    assert(parsed[0].base == "BunnyCloud");
    assert(parsed[0].name == "ExampleAdvancedCloud");
    assert(parsed[0].properties.at("Texture") ==
        "Content/Clouds/ExampleAdvancedCloud");
    assert(bl::csharp::sourceContentDomain("BunnyCloud") == "CLOUD");
    assert(bl::csharp::sourceIdentityForBase("BunnyCloud", "ExampleAdvancedCloud") ==
        "CLOUD:ExampleAdvancedCloud");
    assert(bl::csharp::sourceRequiresNativeRegistration("BunnyCloud"));

    parsed.clear();
    error.clear();
    const std::string missingDefaults =
        "public sealed class NoDefaults : BunnyCloud { }";
    assert(!bl::csharp::parseCSharpSource(missingDefaults, &parsed, &error, true));
    assert(!error.empty());

    std::cout << "CloudLoader C#: source parser identifies CSharp cloud classes OK\n";
}
