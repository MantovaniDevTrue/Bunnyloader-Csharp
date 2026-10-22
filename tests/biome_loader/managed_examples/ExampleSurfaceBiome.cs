using System;
using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

public sealed class ExampleSurfaceBiome : BunnyBiome
{
    public static readonly List<string> Events = new();
    public static int Enters { get; private set; }
    public static int Ticks { get; private set; }
    public static int Leaves { get; private set; }

    public static void ResetCounters()
    {
        Enters = 0;
        Ticks = 0;
        Leaves = 0;
        Events.Clear();
    }

    public override void OnEnter(BunnyBiomeSnapshot snapshot)
    {
        Enters++;
        Events.Add("Enter");
    }

    public override void OnInBiome(BunnyBiomeSnapshot snapshot)
    {
        Ticks++;
        Events.Add("InBiome");
    }

    public override void OnLeave(BunnyBiomeSnapshot snapshot)
    {
        Leaves++;
        Events.Add("Leave");
    }

    public override IReadOnlyList<string> CountedTiles =>
        new[] { "ExampleTile" };

    public override bool IsBiomeActive(BunnyBiomeSnapshot snapshot) =>
        snapshot.TileCount("ExampleTile") >= 40 &&
        snapshot.IsCenterSurface;
}
