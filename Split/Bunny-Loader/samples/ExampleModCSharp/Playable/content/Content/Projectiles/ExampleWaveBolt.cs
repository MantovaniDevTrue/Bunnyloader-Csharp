using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 2;
        TimeLeft = 180;
    }
}
