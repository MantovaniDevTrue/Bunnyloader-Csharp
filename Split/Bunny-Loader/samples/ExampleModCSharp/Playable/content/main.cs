using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 12;
        Height = 12;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 240;
    }
}
