using BunnyLoader.Source;

namespace SourceCSharpMod;

public sealed class SourceBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Source C# Bolt";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 360;
    }
}
