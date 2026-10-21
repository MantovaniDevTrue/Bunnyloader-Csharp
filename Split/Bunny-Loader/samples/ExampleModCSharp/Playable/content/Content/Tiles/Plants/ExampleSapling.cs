using BunnyLoader.Source;

namespace ExampleModCSharp.Tiles.Plants;

public sealed class ExampleSapling : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Tiles/Plants/ExampleSapling.png";
        DisplayName = "Example Sapling";
        Flags = BunnyTileFlags.FrameImportant | BunnyTileFlags.NoAttach |
            BunnyTileFlags.LavaDeath | BunnyTileFlags.TreeSapling |
            BunnyTileFlags.CommonSapling | BunnyTileFlags.SwaysInWindBasic;
        Shape = BunnyTileShape.Sapling1x2;
        SaplingGroundTile = "ExampleTile";
        SaplingVanillaGroundTile = 8;
        SaplingStyleRange = 3;
        SaplingGrowthChance = 20;
        AdjacentTile = 20;
        AddMapEntry(200, 200, 200, "Sapling");
    }
}
