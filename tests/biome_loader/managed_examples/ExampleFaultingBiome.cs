using System;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

public sealed class ExampleFaultingBiome : BunnyBiome
{
    public override bool IsBiomeActive(BunnyBiomeSnapshot snapshot) =>
        throw new InvalidOperationException("Test-only faulty biome");
}
