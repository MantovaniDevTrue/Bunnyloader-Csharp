using System;
using System.IO;
using BunnyLoader.SourceCompiler;
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
        // O mesmo caminho da API de nuvens será usado pelo worker MonoVM.
        // O host só poderá ativá-lo depois de instalar os assets/máscaras.
        var assembly = typeof(CloudContractTests).Assembly;
        BunnyCloudDispatcher.Clear();
        Require(BunnyCloudDispatcher.Register("cloud-test-a", assembly),
            "Managed class registration: " + BunnyCloudDispatcher.LastRegistrationError);
        Require(!BunnyCloudDispatcher.Register("cloud-test-a", assembly),
            "Duplicate mod rejected");
        Require(BunnyCloudDispatcher.Bind("cloud-test-a", nameof(ExampleCommonCloud), 200),
            "Bind common");
        Require(BunnyCloudDispatcher.Bind("cloud-test-a", nameof(ExampleAdvancedCloud), 201),
            "Bind advanced");
        Require(BunnyCloudDispatcher.Bind("cloud-test-a", nameof(ExampleFaultingCloud), 202),
            "Bind faulting probe");
        Require(!BunnyCloudDispatcher.Bind("cloud-test-a", nameof(ExampleCommonCloud), 203),
            "One cloud must not bind twice");
        Require(!BunnyCloudDispatcher.Bind("cloud-test-a", nameof(ExampleAdvancedCloud), 200),
            "Native ID must not have two owners");
        Require(!BunnyCloudDispatcher.IsRare("cloud-test-a", 200), "Common category");
        Require(BunnyCloudDispatcher.IsRare("cloud-test-a", 201), "Rare category");
        Require(BunnyCloudDispatcher.Texture("cloud-test-a", 201) ==
            "Content/Clouds/ExampleAdvancedCloud", "Texture registration");

        Require(BunnyCloudDispatcher.SpawnChance("cloud-test-a", 200, biome) == 1,
            "Managed common chance");
        Require(BunnyCloudDispatcher.SpawnChance("cloud-test-a", 201, biome) == 10,
            "Managed rare chance in biome");
        Require(BunnyCloudDispatcher.SpawnChance("cloud-test-a", 201, outside) == 1,
            "Managed rare chance outside biome");
        Require(BunnyCloudDispatcher.SpawnChance("cloud-test-a", 202, outside) == 0,
            "Faulting chance is suppressed");
        Require(BunnyCloudDispatcher.SpawnChance("cloud-test-a", 202, outside) == 0,
            "Faulting cloud remains blocked");
        Require(BunnyCloudDispatcher.Diagnostics("cloud-test-a").Contains("Faults=1"),
            "Fault is counted only once");

        Require(BunnyCloudDispatcher.TryOnSpawn("cloud-test-a", 201, 8, 0.75f,
            (int)BunnyCloudSpriteEffects.FlipHorizontally, out int adjusted) &&
            adjusted == 0, "Managed OnSpawn disables flipping");
        Require(!BunnyCloudDispatcher.TryOnSpawn("cloud-test-a", 201, 200, 0.75f,
            1, out _), "Invalid slot is rejected");
        Require(!BunnyCloudDispatcher.TryOnSpawn("cloud-test-a", 201, 8,
            float.NaN, 1, out _), "NaN scale is rejected");

        Require(BunnyCloudDispatcher.Register("cloud-test-b", assembly),
            "Second mod allowed");
        Require(!BunnyCloudDispatcher.Bind("cloud-test-b", nameof(ExampleCommonCloud), 200),
            "Native IDs must be unique across mods");
        Require(BunnyCloudDispatcher.Unregister("cloud-test-a"), "Unregister mod A");
        Require(BunnyCloudDispatcher.Bind("cloud-test-b", nameof(ExampleCommonCloud), 200),
            "Native ID reusable after mod A unloads");
        BunnyCloudDispatcher.Clear();
        Require(BunnyCloudDispatcher.ModCount == 0 &&
            BunnyCloudDispatcher.BoundCloudCount == 0, "Worker reset removes registrations");
        // Exercita a inspeção real do compilador Roslyn sobre um assembly
        // contendo apenas BunnyClouds, sem instalar textura, DLL de mod ou APK.
        string probeFolder = Path.Combine(Path.GetTempPath(),
            "bunny-cloud-probe-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(probeFolder);
        try
        {
            string diag = Path.Combine(probeFolder, "source-diagnostics.txt");
            string assemblyPath = typeof(CloudContractTests).Assembly.Location;
            Require(CompiledAssemblyProbe.Verify(assemblyPath, diag) == 3,
                "Compiler probe recognizes three BunnyCloud types");
            string[] lines = File.ReadAllLines(Path.Combine(
                probeFolder, "source-clouds-v1.txt"));
            Require(lines.Length == 4 && lines[0] == "BUNNY_CLOUDS_V1",
                "Compiler emits exactly three cloud metadata rows");
            Require(Array.Exists(lines, line => line ==
                "CLOUD\\tExampleAdvancedCloud\\tContent/Clouds/ExampleAdvancedCloud\\t1".Replace("\\t", "\t")),
                "Compiler preserves rare cloud and texture");
            Require(Array.Exists(lines, line => line ==
                "CLOUD\\tExampleCommonCloud\\tContent/Clouds/ExampleCloud\\t0".Replace("\\t", "\t")),
                "Compiler preserves common cloud and texture");
            Require(File.ReadAllText(diag).Contains("activation=NativeCloudHooksAndMasksNotInstalled"),
                "Compiler never claims the cloud runtime is playable");
        }
        finally
        {
            Directory.Delete(probeFolder, true);
        }

        Console.WriteLine("BunnyCloud C# source-only contract and two examples OK (no APK)");
        return 0;
    }
}
