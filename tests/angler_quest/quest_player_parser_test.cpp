#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<bl::csharp::ParsedSourceType> definitions;
    std::string error;
    const std::string source = R"(namespace ExampleModCSharp;
public sealed class ExampleFishingPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        FishingQuestFishName = "ExampleQuestFish";
        FishingCatchRules = BunnyQuestCatchRules.InvertedGravityAndUncommon;
    }

    public override void UpdateEquips(bool accessoryEquipped) {}
}
)";
    assert(bl::csharp::parseCSharpSource(source, &definitions, &error));
    assert(definitions.size() == 1);
    assert(definitions[0].base == "BunnyPlayer");
    assert(definitions[0].properties.at("FishingQuestFishName") == "ExampleQuestFish");
    assert(definitions[0].properties.at("FishingCatchRules") == "InvertedGravityAndUncommon");
    std::cout << "ANGLER_QUEST_PLAYER_PARSER_OK\n";
    return 0;
}
