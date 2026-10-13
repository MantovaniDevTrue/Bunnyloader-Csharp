using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleVitalPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleVitalAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override int UpdateLifeRegen(bool accessoryEquipped)
    {
        return accessoryEquipped ? 12 : 0;
    }
}
