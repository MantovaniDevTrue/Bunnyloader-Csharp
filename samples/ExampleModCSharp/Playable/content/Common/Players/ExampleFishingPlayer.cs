using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFishingPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        FishingQuestFishName = "ExampleQuestFish";
        FishingCatchRules = BunnyQuestCatchRules.InvertedGravityAndUncommon;
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }
}
