using BunnyLoader.Source;

namespace ExampleModCSharp.Tiles.Plants;

public sealed class ExampleTree : BunnyTree
{
    public override void SetDefaults()
    {
        Texture = "Content/Tiles/Plants/ExampleTree";
        GrowsOnTile = "ExampleTile";
        SaplingTile = "ExampleSapling";
        WoodItem = "ExampleItem";
        ShakeItem = "ExampleTileItem";
        ShakeLeaves = true;
        CanDropAcorn = true;
        LeafGoreName = "ExampleTree_Leaf";
        DustType = 7;
    }
}
