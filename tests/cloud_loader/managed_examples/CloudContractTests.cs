using System;
using BunnyLoader.Source;
using ExampleModCSharp.CloudExamples;

static class CloudContractTests
{
    static void Require(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }

    static void Reject<T>(Action action) where T : Exception
    {
        try { action(); }
        catch (T) { return; }
        throw new Exception("Expected " + typeof(T).Name);
    }

    static int Main()
    {
        var common = new ExampleCommonCloud();
        var rare = new ExampleAdvancedCloud();
        common.SetDefaults();
        rare.SetDefaults();
        common.Validate();
        rare.Validate();
        Require(!common.RareCloud && rare.RareCloud, "Rare versus common");
        var biome = new BunnyCloudEnvironment(false, false,
            new[] { "ExampleModCSharp/ExampleSurfaceBiome" });
        var outside = new BunnyCloudEnvironment(false, true);
        var menu = new BunnyCloudEnvironment(true, false,
            new[] { "ExampleModCSharp/ExampleSurfaceBiome" });
        Require(common.SpawnChance(biome) == 1, "Common weight");
        Require(rare.SpawnChance(biome) == 10, "Inside biome weight");
        Require(rare.SpawnChance(outside) == 1, "Outside biome weight");
        Require(rare.SpawnChance(menu) == 1, "Menu cannot contain biome");
        Require(outside.TenthAnniversaryWorld, "Anniversary property");
        var spawn = new BunnyCloudSpawn(60, 199, 0.9f,
            BunnyCloudSpriteEffects.FlipHorizontally);
        rare.OnSpawn(spawn);
        Require(spawn.SpriteEffects == BunnyCloudSpriteEffects.None,
            "OnSpawn must stop horizontal flipping");
        Require(spawn.HasValidVisuals, "SpriteEffects snapshot validation");
        Reject<ArgumentOutOfRangeException>(() =>
            new BunnyCloudSpawn(60, 200, 1, BunnyCloudSpriteEffects.None));
        Reject<ArgumentOutOfRangeException>(() =>
            new BunnyCloudSpawn(60, 0, float.NaN, BunnyCloudSpriteEffects.None));
        Reject<ArgumentException>(() =>
            new BunnyCloudEnvironment(false, false, new[] { "" }));
        Require(BunnyCloud.SafeSpawnChance(float.NaN) == 0, "NaN disabled");
        Require(BunnyCloud.SafeSpawnChance(float.PositiveInfinity) == 0,
            "Infinity disabled");
        Require(BunnyCloud.SafeSpawnChance(-2) == 0, "Negative disabled");
        Require(BunnyCloud.SafeSpawnChance(10) == 10, "Valid weight preserved");
        Console.WriteLine("BunnyCloud C# source-only contract and two examples OK (no APK)");
        return 0;
    }
}
