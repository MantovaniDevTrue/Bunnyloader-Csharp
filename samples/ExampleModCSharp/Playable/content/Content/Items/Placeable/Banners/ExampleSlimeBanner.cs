using BunnyLoader.Source;

namespace ExampleModCSharp.Items.Placeable.Banners;

public sealed class ExampleSlimeBanner : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/Placeable/Banners/ExampleSlimeBanner.png";
        DisplayName = "Example Slime Banner";
        Width = 10;
        Height = 24;
        MaxStack = 9999;
        Value = 10;
        Tooltip = "Uma bandeira do Example Slime.";
        DefaultToPlaceableTile("ExampleEnemyBannerTile", 1);
    }
}
