#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    using namespace bl::csharp;
    std::vector<ParsedSourceType> definitions;
    std::string error;
    const std::string source = R"(
        namespace ExampleModCSharp.Achievements;
        public sealed class FiveKills : BunnyAchievement
        {
            private BunnyAchievementCondition? count;
            public override void SetStaticDefaults()
            {
                FriendlyName = "Cinco mortes";
                Texture = "Content/Achievements/FiveKills.png";
                count = AddIntCondition("Kills", 5);
            }
            public override void OnNPCKilled(int type)
            {
                if (type == 10) count!.Increase();
            }
        }
    )";
    assert(parseCSharpSource(source, &definitions, &error, true));
    assert(definitions.size() == 1);
    assert(definitions[0].name == "FiveKills");
    assert(definitions[0].base == "BunnyAchievement");
    assert(definitions[0].namespaceName == "ExampleModCSharp.Achievements");
    assert(sourceIdentityForBase("BunnyAchievement", "FiveKills") == "ACHIEVEMENT:FiveKills");
    assert(!sourceRequiresNativeRegistration("BunnyAchievement"));
    assert(sourceRequiresNativeRegistration("BunnyItem"));

    definitions.clear();
    const std::string mixed = R"(
        public sealed class MyItem : BunnyItem {
            public override void SetDefaults() {
                Texture = "Content/Items/MyItem.png";
            }
        }
        public sealed class MyAchievement : BunnyAchievement {
            public override void SetStaticDefaults() {
                Texture = "Content/Achievements/MyAchievement.png";
                AddCondition();
            }
        }
    )";
    assert(parseCSharpSource(mixed, &definitions, &error, true));
    assert(definitions.size() == 2);
    assert(definitions[0].base == "BunnyItem");
    assert(definitions[1].base == "BunnyAchievement");

    std::cout << "CSHARP_ACHIEVEMENT_SOURCE_PARSER_PASS=YES\n";
}
