namespace BunnyLoader.Source;

/// <summary>
/// Base de bandeira do ExampleModBunny: 1x3, pendurada, com estilos horizontais.
/// O host C# configura AnchorData para blocos solidos e plataformas no registro.
/// </summary>
public abstract class BunnyBannerTile : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Shape = BunnyTileShape.HangingBanner1x3;
        Flags = BunnyTileFlags.FrameImportant |
                BunnyTileFlags.NoAttach |
                BunnyTileFlags.LavaDeath |
                BunnyTileFlags.DisableSmartCursor;
        CoordinateHeight = 18;
        AddMapEntry(13, 88, 130, "MapObject.Banner");
    }
}
