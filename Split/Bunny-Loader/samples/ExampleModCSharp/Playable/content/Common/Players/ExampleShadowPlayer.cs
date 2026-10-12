using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleShadowPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleShadowAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override bool FrameEffects(bool accessoryEquipped)
    {
        return accessoryEquipped;
    }
}
