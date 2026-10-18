using BunnyLoader.Source;

namespace SourceCSharpMod;

public sealed class SourceSpark : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Source C# Spark";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 14;
        Height = 14;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 2;
        TimeLeft = 240;
    }
}
