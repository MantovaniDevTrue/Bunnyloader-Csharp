using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleEnemyBannerTile : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Tiles/Banners/ExampleEnemyBannerTile.png";
        DisplayName = "Example Slime Banner";
        Shape = BunnyTileShape.HangingBanner1x3;
        CoordinateHeight = 18;
        Flags = BunnyTileFlags.FrameImportant |
                BunnyTileFlags.NoAttach |
                BunnyTileFlags.LavaDeath |
                BunnyTileFlags.DisableSmartCursor;
        AddMapEntry(13, 88, 130, "MapObject.Banner");

        // O suporte nativo utiliza TileObjectData.Style1x2Top, altura 3,
        // CoordinateHeights [16,16,16], estilo horizontal e DrawYOffset -2.
        // AnchorTop com alternativa de plataforma, NearbyEffects e
        // SpecialPoint do desenho dependem da proxima etapa nativa.
    }
}
