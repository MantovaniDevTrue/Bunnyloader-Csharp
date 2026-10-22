using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleEnemyBannerTile : BunnyBannerTile
{
    public override void SetStaticDefaults()
    {
        base.SetStaticDefaults();
        Texture = "Content/Tiles/Banners/ExampleEnemyBannerTile.png";
        DisplayName = "Example Slime Banner";
    }
}
