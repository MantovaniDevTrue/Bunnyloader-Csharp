using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleFocusAccessory);
    }

    public override void ResetEffects()
    {
        DefenseBonus = 0;
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            DefenseBonus = 7;
    }
}
