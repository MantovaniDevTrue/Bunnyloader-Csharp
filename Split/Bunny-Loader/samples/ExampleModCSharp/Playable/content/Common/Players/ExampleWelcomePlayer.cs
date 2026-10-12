using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWelcomePlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleWelcomeAccessory);
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override string OnEnterWorld()
    {
        return "Example C#: BunnyPlayer OnEnterWorld ativo!";
    }
}
