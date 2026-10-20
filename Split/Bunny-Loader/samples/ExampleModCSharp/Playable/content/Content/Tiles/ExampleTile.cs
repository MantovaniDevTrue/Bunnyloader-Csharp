using BunnyLoader.Source;

namespace ExampleModCSharp.Tiles;

public sealed class ExampleTile : BunnyTile {
    public override void SetStaticDefaults() {
        Texture = "Content/Tiles/ExampleTile.png";
        DisplayName = "Example Block";
        Flags = BunnyTileFlags.Solid | BunnyTileFlags.BlockLight |
            BunnyTileFlags.MergeDirt | BunnyTileFlags.ChecksForMerge;
        DustName = "Stone";
        AddMapEntry(0, 200, 255, "ExampleTile");
    }
}
