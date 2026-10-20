using BunnyLoader.Source;

namespace ExampleModCSharp;

public abstract class InheritedQuestFish : BunnyItem
{
    public override void SetDefaults()
    {
        if (DisplayName != "Attribute Quest Fish" ||
            Texture != "Content/Items/Quests/ExampleQuestFish.png")
            throw new System.InvalidOperationException("BunnyContent identity missing before quest defaults");
        DefaultToQuestFish();
        QuestCanCatchWhen = context => context.FishType == 954 && context.PlayerIndex == 2;
    }
}

[BunnyContent(BunnyContentKind.Item, "Attribute Quest Fish", "Content/Items/Quests/ExampleQuestFish.png")]
public sealed class AttributedQuestFish : InheritedQuestFish
{
}
