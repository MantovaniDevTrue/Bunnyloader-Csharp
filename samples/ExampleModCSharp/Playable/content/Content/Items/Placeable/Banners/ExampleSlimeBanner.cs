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

        // No tModLoader, este item usa DefaultToPlaceableTile do tile 1x3.
        // O BunnyTile atual so suporta Single e Style2x1. Nao substituir
        // por um tile 1x1 nem pelo banner vanilla: isso alteraria o visual.
        // A colocacao real sera ativada com o tile 1x3 nativo completo.
    }
}
