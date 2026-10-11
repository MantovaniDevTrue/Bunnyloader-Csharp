using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 12;
        UseTime = 16;
        ShootSpeed = 9f;
        Shoot = nameof(ExampleWaveBolt);
    }
}
