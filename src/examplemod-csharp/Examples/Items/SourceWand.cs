using BunnyLoader.Source;

namespace SourceCSharpMod;

public sealed class SourceWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Source C# Wand";
        Damage = 22;
        UseTime = 18;
        ShootSpeed = 13f;
        Shoot = nameof(SourceBolt);
    }
}
