using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMomentumPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleMomentumAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override int UpdateMovement(bool accessoryEquipped)
    {
        return accessoryEquipped ? 6 : 0;
    }
}
