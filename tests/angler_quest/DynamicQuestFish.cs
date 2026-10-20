using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class DynamicQuestFish : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Dynamic Quest Fish";
        Texture = "Content/Items/Quests/ExampleQuestFish.png";
        DefaultToQuestFish();
        QuestAvailableWhen = context => context.Hardmode && context.FishType == 950;
        QuestCanCatchWhen = context => context.InvertedGravity && context.Uncommon && (context.PlayerIndex == -1 || context.PlayerIndex == 2);
    }
}
