using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleTrailblazerBootsPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleTrailblazerBootsAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            MoveSpeedPercent = 12;
    }

    public override int UpdateMovement(bool accessoryEquipped)
    {
        return accessoryEquipped ? 8 : 0;
    }

}
