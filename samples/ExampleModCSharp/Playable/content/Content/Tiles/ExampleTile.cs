using BunnyLoader.Source;

namespace ExampleModCSharp;

// Porta do ModTile ExampleTile.js: as quatro flags atuam nos arrays
// nativos do Terraria e nao exigem uma chamada C# por tile ou por quadro.
public sealed class ExampleTile : BunnyTile
{
    public override void SetStaticDefaults()
    {
        DisplayName = "ExampleTile";
        Texture = "Content/Tiles/ExampleTile.png";
        DustName = "Stone";
        Flags = BunnyTileFlags.Solid | BunnyTileFlags.BlockLight |
            BunnyTileFlags.MergeDirt | BunnyTileFlags.ChecksForMerge;
        AddMapEntry(0, 200, 255, nameof(ExampleTile));
    }
}
