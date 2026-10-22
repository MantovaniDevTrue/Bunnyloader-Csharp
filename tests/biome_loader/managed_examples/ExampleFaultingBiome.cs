using System;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

public sealed class ExampleFaultingBiome : BunnyBiome
{
    public static bool Activate { get; set; }
    public static int Enters { get; private set; }
    public static int Inside { get; private set; }
    public static int Leaves { get; private set; }

    public override bool IsBiomeActive(BunnyBiomeSnapshot snapshot)
    {
        if (!Activate) throw new InvalidOperationException("Test-only faulty biome");
        return true;
    }

    public override void OnEnter(BunnyBiomeSnapshot snapshot)
    {
        Enters++;
        throw new InvalidOperationException("Test-only OnEnter failure");
    }

    public override void OnInBiome(BunnyBiomeSnapshot snapshot)
    {
        Inside++;
        throw new InvalidOperationException("Test-only OnInBiome failure");
    }

    public override void OnLeave(BunnyBiomeSnapshot snapshot)
    {
        Leaves++;
        throw new InvalidOperationException("Test-only OnLeave failure");
    }
}
