using BunnyLoader.Source;

namespace ExampleTiles.Furniture;

public sealed class ExampleWorkbench : BunnyTile {
    public override void SetStaticDefaults() {
        Texture = "Content/Tiles/Furniture/ExampleWorkbench.png";
        DisplayName = "Example Workbench";
        Flags = BunnyTileFlags.Table | BunnyTileFlags.SolidTop |
            BunnyTileFlags.NoAttach | BunnyTileFlags.LavaDeath |
            BunnyTileFlags.FrameImportant | BunnyTileFlags.DisableSmartCursor |
            BunnyTileFlags.HasOutlines | BunnyTileFlags.IgnoredByNpcStepUp |
            BunnyTileFlags.CountsAsTable;
        Shape = BunnyTileShape.Style2x1;
        CoordinateHeight = 18;
        DustName = "Platinum";
        AdjacentTile = 18;
        AddMapEntry(200, 200, 200, "ExampleWorkbench");
    }

    public override int NumDust(bool fail) => fail ? 1 : 3;
}
