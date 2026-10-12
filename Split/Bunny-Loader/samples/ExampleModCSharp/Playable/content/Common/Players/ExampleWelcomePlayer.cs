using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWelcomePlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override string OnEnterWorld()
    {
        return "Example C#: BunnyPlayer OnEnterWorld ativo!";
    }

    public override string OnRespawn()
    {
        return "Example C#: BunnyPlayer OnRespawn ativo!";
    }
}
