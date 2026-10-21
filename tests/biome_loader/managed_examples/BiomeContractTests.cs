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
        Require(BunnyBiomeDispatcher.RegisterFromPath("example-mod",
            typeof(BiomeContractTests).Assembly.Location),
            BunnyBiomeDispatcher.LastError);
        Require(!BunnyBiomeDispatcher.RegisterFromPath("example-mod",
            typeof(BiomeContractTests).Assembly.Location),
            "Cannot register same mod twice");
        string deps = BunnyBiomeDispatcher.TilesWire("example-mod");
        Require(deps.StartsWith("BUNNY_BIOME_TILES_V1\n", StringComparison.Ordinal),
            "Biome dependency protocol");
        Require(deps.Contains("BIOME\tExampleSurfaceBiome\tExampleTile\n",
            StringComparison.Ordinal), "Biome tile dependency matches ExampleModBunny");

        string active = BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=40");
        Require(active == "BUNNY_BIOMES_V1\nBIOME\tExampleSurfaceBiome\n",
            "Dispatcher reports only the authentic active biome");
        Require(BunnyBiomeDispatcher.LastError.Contains(
            "ExampleFaultingBiome.IsBiomeActive", StringComparison.Ordinal),
            "Faulting biome isolated without affecting ExampleSurfaceBiome");
        Require(BunnyBiomeDispatcher.EvaluateWire(
            "example-mod", false, true, 3200 * 16f, 6400, false, true,
            "ExampleTile=39") == "BUNNY_BIOMES_V1\n",
            "Dispatcher disallows insufficient block count");
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
        Require(BunnyBiomeDispatcher.ModCount == 0, "No stale biome registrations");
        Console.WriteLine("BunnyBiome CSharp policy, wire, Roslyn and isolation OK (no APK)");
        return 0;
    }
}
