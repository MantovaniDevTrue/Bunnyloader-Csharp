using BunnyLoader.Source;

namespace SourceCSharpMod;

public sealed class SourceStaff : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Source C# Staff";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 30;
        UseTime = 24;
        ShootSpeed = 10f;
        Shoot = nameof(SourceSpark);
    }
}
