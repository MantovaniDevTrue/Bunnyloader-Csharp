using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 18;
        UseTime = 20;
        ShootSpeed = 11f;
        Shoot = nameof(ExampleBolt);
    }
}
