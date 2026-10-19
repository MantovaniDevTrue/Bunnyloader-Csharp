using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleQuestFish : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Upside-down Fish";
        Texture = "Content/Items/Quests/ExampleQuestFish.png";
        DefaultToQuestFish();
        QuestAvailableInPreHardmode = false;
        QuestAvailableInHardmode = true;
        QuestCatchRules = BunnyQuestCatchRules.InvertedGravity | BunnyQuestCatchRules.Uncommon;
        QuestDescription = "I've heard stories of a fish that swims upside-down. Supposedly you have to stand upside-down yourself to even find one. One of those would go great on my ceiling. Go fetch!";
        QuestCatchLocation = "Caught anywhere while standing upside-down.";
    }
}
