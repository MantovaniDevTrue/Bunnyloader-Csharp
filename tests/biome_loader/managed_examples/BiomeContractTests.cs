using System;
using System.Collections.Generic;
using System.IO;
using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using ExampleModCSharp.Biomes;

static class BiomeContractTests
{
    static void Require(bool okay, string message)
    {
        if (!okay) throw new Exception(message);
    }

    static void Reject<T>(Action callback) where T : Exception
    {
        try { callback(); }
        catch (T) { return; }
        throw new Exception("Expected " + typeof(T).Name);
    }

    static int Main()
    {
        BunnyBiomeSnapshot facts = new(false, true, false, true,
            3200f * 16f, 6400,
            new Dictionary<string, int> { ["ExampleTile"] = 40 });
        var example = new ExampleSurfaceBiome();
        example.Validate();
        Require(example.IsBiomeActive(facts), "Original 40 tiles + center + surface");
        Require(!example.IsBiomeActive(new BunnyBiomeSnapshot(
            false, true, false, true, 3200 * 16f, 6400,
            new Dictionary<string, int> { ["ExampleTile"] = 39 })),
            "Thirty-nine tiles do not activate biome");
        Require(!example.IsBiomeActive(new BunnyBiomeSnapshot(
            false, true, false, true, 0, 6400,
            new Dictionary<string, int> { ["ExampleTile"] = 40 })),
            "Outside center never activates biome");
        Require(!example.IsBiomeActive(new BunnyBiomeSnapshot(
            false, true, false, false, 3200 * 16f, 6400,
            new Dictionary<string, int> { ["ExampleTile"] = 40 })),
            "Underground is not surface");
        Require(!example.IsBiomeActive(new BunnyBiomeSnapshot(
            true, true, false, true, 3200 * 16f, 6400,
            new Dictionary<string, int> { ["ExampleTile"] = 40 })),
            "Main menu cannot be inside mod biome");
        Require(facts.TileCount("MissingTile") == -1,
            "Unavailable counter must not be guessed");
        Reject<ArgumentException>(() => new BunnyBiomeSnapshot(
            false, true, true, false, float.NaN, 6400,
            new Dictionary<string, int>()));
        Reject<ArgumentException>(() => new BunnyBiomeSnapshot(
            false, true, true, false, 0, 6400,
            new Dictionary<string, int> { ["../../unknown"] = 50 }));

        BunnyBiomeDispatcher.Clear();
        ExampleSurfaceBiome.ResetCounters();
        Require(BunnyBiomeDispatcher.RegisterFromPath("example-mod",
            typeof(BiomeContractTests).Assembly.Location),
            BunnyBiomeDispatcher.LastError);
        Require(!BunnyBiomeDispatcher.RegisterFromPath("example-mod",
            typeof(BiomeContractTests).Assembly.Location),
            "Cannot register same mod twice");
        string deps = BunnyBiomeDispatcher.TilesWire("example-mod");
        Require(deps.StartsWith("BUNNY_BIOMES_V1\n", StringComparison.Ordinal),
            "Biome dependency protocol");
        Require(deps.Contains("BIOME\tExampleSurfaceBiome\tExampleTile\n",
            StringComparison.Ordinal), "Biome tile dependency matches ExampleModBunny");

        string active = BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40");
        Require(active == "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\n",
            "Dispatcher reports only the authentic active biome");
        Require(ExampleSurfaceBiome.Enters == 1 &&
                ExampleSurfaceBiome.Ticks == 1 &&
                ExampleSurfaceBiome.Leaves == 0 &&
                string.Join(",", ExampleSurfaceBiome.Events) ==
                "Enter,InBiome",
            "OnEnter precedes OnInBiome on first active evaluation");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40") == active &&
            ExampleSurfaceBiome.Enters == 1 &&
            ExampleSurfaceBiome.Ticks == 2,
            "OnInBiome repeats on subsequent valid evaluation, not OnEnter");
        Require(BunnyBiomeDispatcher.LastError.Contains(
            "ExampleFaultingBiome.IsBiomeActive", StringComparison.Ordinal),
            "Faulting biome isolated without affecting ExampleSurfaceBiome");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=39") == "BUNNY_BIOMES_V1\n",
            "Dispatcher disallows insufficient block count");
        Require(ExampleSurfaceBiome.Enters == 1 &&
                ExampleSurfaceBiome.Ticks == 2 &&
                ExampleSurfaceBiome.Leaves == 1 &&
                string.Join(",", ExampleSurfaceBiome.Events) ==
                "Enter,InBiome,InBiome,Leave",
            "OnLeave emitted precisely once on transition to false");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "") == "BUNNY_BIOMES_V1\n",
            "Missing counter disables the biome");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40;ExampleTile=99") == "",
            "Duplicate counters rejected transactionally");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=-1") == "",
            "Negative counter rejected");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=2147483647") == "",
            "Oversized counter rejected");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", true, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40") == "BUNNY_BIOMES_V1\n",
            "World unload cannot preserve active mod biome");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40") == active &&
            ExampleSurfaceBiome.Enters == 2 &&
            ExampleSurfaceBiome.Ticks == 3,
            "Reentry fires after leaving the biome");
        BunnyBiomeDispatcher.ResetWorld();
        BunnyBiomeDispatcher.ResetWorld();
        Require(ExampleSurfaceBiome.Leaves == 2 &&
                string.Join(",", ExampleSurfaceBiome.Events) ==
                "Enter,InBiome,InBiome,Leave,Enter,InBiome,Leave",
            "World reset emits one final OnLeave, never duplicate");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40") == active &&
            ExampleSurfaceBiome.Enters == 3,
            "World reset clears active biome and allows entering again");


        ExampleFaultingBiome.Activate = true;
        string recovered = BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40");
        Require(recovered.Contains("BIOME\tExampleFaultingBiome\n",
                StringComparison.Ordinal) &&
                recovered.Contains("BIOME\tExampleSurfaceBiome\n",
                StringComparison.Ordinal) &&
                ExampleFaultingBiome.Enters == 1 &&
                ExampleFaultingBiome.Inside == 1,
            "One failing IsBiomeActive does not permanently disable biome");
        Require(BunnyBiomeDispatcher.LastError.Contains(
                "ExampleFaultingBiome.OnInBiome", StringComparison.Ordinal),
            "OnInBiome exception isolated from active flags");
        recovered = BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40");
        Require(ExampleFaultingBiome.Enters == 1 &&
                ExampleFaultingBiome.Inside == 2,
            "Throwing OnEnter must not repeat on every evaluation");
        ExampleFaultingBiome.Activate = false;
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40") == active &&
            ExampleFaultingBiome.Leaves == 1,
            "Throwing OnLeave must not corrupt other active biomes");
        Require(BunnyBiomeDispatcher.LastError.Contains(
                "ExampleFaultingBiome.IsBiomeActive", StringComparison.Ordinal),
            "Biome IsBiomeActive errors remain recoverable and isolated");

        string dir = Path.Combine(Path.GetTempPath(),
            "bunny-biome-source-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        try
        {
            string diagnostics = Path.Combine(dir, "source-diagnostics.txt");
            Require(CompiledAssemblyProbe.Verify(
                typeof(BiomeContractTests).Assembly.Location, diagnostics) == 2,
                "Roslyn discovers two BunnyBiome classes");
            string[] manifest = File.ReadAllLines(
                Path.Combine(dir, "source-biomes-v1.txt"));
            Require(manifest.Length == 3 && manifest[0] == "BUNNY_BIOMES_V1",
                "Compiler writes versioned biome manifest");
            Require(Array.Exists(manifest, x =>
                x == "BIOME\tExampleSurfaceBiome\tExampleTile"),
                "Compiler retains declared required tile");
            Require(File.ReadAllText(diagnostics).Contains(
                "activation=NativeBiomeRegistryNotInstalled",
                StringComparison.Ordinal),
                "Never mark CSharp biome playable in source probe");
        }
        finally
        {
            Directory.Delete(dir, true);
        }

        BunnyBiomeDispatcher.Clear();
        Require(ExampleSurfaceBiome.Leaves == 3,
            "Managed unload calls OnLeave once for active biome");
        Require(BunnyBiomeDispatcher.ModCount == 0, "No stale biome registrations");
        Console.WriteLine("BunnyBiome CSharp policy, wire, Roslyn and isolation OK (no APK)");
        return 0;
    }
}
