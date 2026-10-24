#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentKinds.generated.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using bl::csharp::ParsedSourceType;
using bl::csharp::parseCSharpSource;

bool expect(const std::string& source, const std::string& kind, bool expected = true) {
    std::vector<ParsedSourceType> definitions;
    std::string error;
    bool ok = parseCSharpSource(source, &definitions, &error);
    if (ok != expected || (ok && (definitions.size() != 1 || definitions[0].base != kind))) {
        std::cerr << "Unexpected parser result: " << error << "\n";
        return false;
    }
    return true;
}

int main() {
    using namespace bl::csharp;
    static_assert(kSourceContentKinds.size() == 12);
    static_assert(kSourceNativeRegistrationOrder.size() == 11);
    assert(isKnownSourceKind("Item") && isKnownSourceKind("Projectile"));
    assert(isKnownSourceKind("NPC") && isKnownSourceKind("Player"));
    assert(!isKnownSourceKind("Unknown"));
    assert(isRecognizedSourceBase("BunnyPlayer"));
    assert(std::string(kSourceNativeRegistrationOrder[0]) == "BunnyDust");
    assert(std::string(kSourceNativeRegistrationOrder[10]) == "BunnyPrefix");

    const std::array<std::pair<std::string, std::string>, 4> samples = {{
        {"public class SampleItem : BunnyItem { public override void SetDefaults() { DisplayName = \"Item\"; Texture = \"Content/Items/SampleItem.png\"; } }", "BunnyItem"},
        {"public class SampleBolt : BunnyProjectile { public override void SetDefaults() { DisplayName = \"Bolt\"; Texture = \"Content/Projectiles/SampleBolt.png\"; } }", "BunnyProjectile"},
        {"public class SampleSlime : BunnyNPC { public override void SetDefaults() { DisplayName = \"Slime\"; Texture = \"Content/NPCs/SampleSlime.png\"; } }", "BunnyNPC"},
        {"public class SamplePlayer : BunnyPlayer { public override void SetDefaults() { } }", "BunnyPlayer"},
    }};
    for (const auto& sample : samples) if (!expect(sample.first, sample.second)) return 1;
    const std::string attributed =
        "[BunnyContent(BunnyContentKind.Item, \"Named\", \"Content/Items/Named.png\")] "
        "public class Named : BunnyItem { public override void SetDefaults() { } }";
    if (!expect(attributed, "BunnyItem")) return 2;
    const std::string invalid =
        "[BunnyContent(BunnyContentKind.Invented, \"Invalid\", \"Content/Items/Invalid.png\")] "
        "public class Invalid : BunnyItem { public override void SetDefaults() { } }";
    if (!expect(invalid, "", false)) return 3;
    std::cout << "CONTENT_CONTRACT_PARSER_SMOKE PASS\n";
}
