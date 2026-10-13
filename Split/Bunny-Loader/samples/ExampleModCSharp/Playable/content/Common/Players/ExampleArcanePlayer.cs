using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleArcanePlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleArcaneAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override int UpdateManaRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 10 : 0;
    }
}
