using System;
using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

public sealed class ExampleSurfaceBiome : BunnyBiome
{
    public override IReadOnlyList<string> CountedTiles =>
        new[] { "ExampleTile" };

    public override bool IsBiomeActive(BunnyBiomeSnapshot snapshot) =>
        snapshot.TileCount("ExampleTile") >= 40 &&
        snapshot.IsCenterSurface;
}
