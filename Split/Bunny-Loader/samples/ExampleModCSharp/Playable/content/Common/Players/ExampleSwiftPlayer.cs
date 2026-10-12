using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSwiftPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleSwiftAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            MoveSpeedPercent = 8;
    }
}
