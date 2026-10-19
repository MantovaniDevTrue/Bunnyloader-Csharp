using BunnyLoader.Source;

var passed = 0;
var failed = 0;

void Check(string name, Action run)
{
    try
    {
        run();
        passed++;
        Console.WriteLine("PASS " + name);
    }
    catch (Exception exception)
    {
        failed++;
        Console.Error.WriteLine("FAIL " + name + ": " + exception);
    }
}

void Equal<T>(T expected, T actual)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new Exception("expected=" + expected + ", actual=" + actual);
}

void Throws<T>(Action run) where T : Exception
{
    try { run(); }
    catch (T) { return; }
    throw new Exception("Expected " + typeof(T).Name);
}

Check("ids.begin-after-vanilla-and-lookup-by-mod", () =>
{
    var registry = new BunnyRarityRegistry();
    Equal(12, registry.RarityCount);
    Equal(12, registry.Register("examplemodcsharp", new RisingRarity()));
    Equal(13, registry.Register("examplemodcsharp", new FallingRarity()));
    Equal(14, registry.RarityCount);
    Equal(12, registry.TypeOf<RisingRarity>("examplemodcsharp"));
    Equal(13, registry.TypeOf<FallingRarity>("examplemodcsharp"));
    Throws<KeyNotFoundException>(() => registry.TypeOf<RisingRarity>("othermod"));
});

Check("ids.same-class-isolated-between-mods", () =>
{
    var registry = new BunnyRarityRegistry();
    Equal(12, registry.Register("mod-a", new RisingRarity()));
    Equal(13, registry.Register("mod-b", new RisingRarity()));
    Equal(12, registry.TypeOf<RisingRarity>("mod-a"));
    Equal(13, registry.TypeOf<RisingRarity>("mod-b"));
});

Check("registration.reject-duplicate-and-instance-reuse", () =>
{
    var registry = new BunnyRarityRegistry();
    var rarity = new RisingRarity();
    Equal(12, registry.Register("example", rarity));
    Throws<InvalidOperationException>(() => registry.Register("example", rarity));
    Throws<InvalidOperationException>(() => registry.Register("example", new RisingRarity()));
    Throws<InvalidOperationException>(() => new BunnyRarityRegistry().Register("other", rarity));
    Equal(13, registry.RarityCount);
});

Check("registration.reject-invalid-ids-and-null", () =>
{
    var registry = new BunnyRarityRegistry();
    foreach (var invalid in new[] { "", "mod/name", "mod\tname", "bad id", new string('x', 97) })
        Throws<ArgumentException>(() => registry.Register(invalid, new RisingRarity()));
    Throws<ArgumentNullException>(() => registry.Register("mod", null!));
    Equal(12, registry.RarityCount);
});

Check("registration.seal-calls-once-and-disallows-new-entries", () =>
{
    var registry = new BunnyRarityRegistry();
    var rarity = new RisingRarity();
    registry.Register("mod", rarity);
    var published = registry.Seal();
    Equal(1, rarity.SetupCalls);
    Equal(true, ReferenceEquals(published, registry.Seal()));
    Equal(1, rarity.SetupCalls);
    Equal(1, published.Count);
    Equal(12, published[0].Type);
    Throws<InvalidOperationException>(() => registry.Register("mod", new FallingRarity()));
    Throws<NotSupportedException>(() => ((IList<BunnyRarityEntry>)published)[0] = published[0]);
});

Check("registration.static-defaults-failure-does-not-publish", () =>
{
    var registry = new BunnyRarityRegistry();
    registry.Register("mod", new FailingRarity());
    Throws<InvalidOperationException>(() => registry.Seal());
    Throws<InvalidOperationException>(() => registry.TryGetColor(12, out _));
    Throws<InvalidOperationException>(() => registry.Register("mod", new FallingRarity()));
});

Check("color.dynamic-getter-and-vanilla-fallback", () =>
{
    var registry = new BunnyRarityRegistry();
    var rarity = new RisingRarity();
    Equal(12, registry.Register("mod", rarity));
    Throws<InvalidOperationException>(() => registry.TryGetColor(12, out _));
    registry.Seal();
    Equal(false, registry.TryGetColor(11, out _));
    Equal(false, registry.TryGetColor(9000, out _));
    Equal(true, registry.TryGetColor(12, out BunnyColor color));
    Equal((byte)40, color.R);
    rarity.Red = 150;
    Equal(true, registry.TryGetColor(12, out color));
    Equal((byte)150, color.R);
    Equal((byte)255, color.A);
});

Check("prefix.exact-boundary-offsets", () =>
{
    Equal(-2, BunnyRarityRegistry.PrefixOffset(0.8f));
    Equal(-1, BunnyRarityRegistry.PrefixOffset(0.81f));
    Equal(-1, BunnyRarityRegistry.PrefixOffset(0.95f));
    Equal(0, BunnyRarityRegistry.PrefixOffset(1.0f));
    Equal(1, BunnyRarityRegistry.PrefixOffset(1.05f));
    Equal(1, BunnyRarityRegistry.PrefixOffset(1.19f));
    Equal(2, BunnyRarityRegistry.PrefixOffset(1.2f));
});

Check("prefix.vanilla-preserves-native-result", () =>
{
    var registry = new BunnyRarityRegistry();
    registry.Register("mod", new RisingRarity());
    registry.Seal();
    Equal(6, registry.ResolvePrefixedRarity(5, 1.2f, 6));
    Equal(9, registry.ResolvePrefixedRarity(9999, 1.2f, 9));
});

Check("prefix.cross-rarity-transitions-and-default", () =>
{
    var registry = new BunnyRarityRegistry();
    var lower = new RisingRarity();
    var higher = new FallingRarity();
    var lowId = registry.Register("mod", lower);
    var highId = registry.Register("mod", higher);
    lower.HigherId = highId;
    higher.LowerId = lowId;
    registry.Seal();
    Equal(lowId, registry.ResolvePrefixedRarity(lowId, 1.0f, 11));
    Equal(highId, registry.ResolvePrefixedRarity(lowId, 1.2f, 11));
    Equal(lowId, registry.ResolvePrefixedRarity(highId, 0.8f, 11));
    Equal(highId, registry.ResolvePrefixedRarity(highId, 1.2f, 11));
});

Check("prefix.clamps-except-special-negative-sentinels", () =>
{
    var registry = new BunnyRarityRegistry();
    var rarity = new FixedRarity { Result = 99999 };
    var type = registry.Register("mod", rarity);
    registry.Seal();
    Equal(type, registry.ResolvePrefixedRarity(type, 1.0f, 11));
    rarity.Result = -10;
    Equal(-1, registry.ResolvePrefixedRarity(type, 1.0f, 11));
    rarity.Result = -11;
    Equal(-11, registry.ResolvePrefixedRarity(type, 1.0f, 11));
});

Check("prefix.nonfinite-and-negative-multipliers-rejected", () =>
{
    var registry = new BunnyRarityRegistry();
    var type = registry.Register("mod", new RisingRarity());
    registry.Seal();
    foreach (float invalid in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity, -0.1f })
    {
        Throws<ArgumentOutOfRangeException>(() => BunnyRarityRegistry.PrefixOffset(invalid));
        Throws<ArgumentOutOfRangeException>(() => registry.ResolvePrefixedRarity(type, invalid, 11));
    }
});

Check("compiler.emits-rarity-declarations-without-executing-static-defaults", () =>
{
    var scratch = Path.Combine(Path.GetTempPath(), "bunny-rarity-smoke-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(scratch);
    try
    {
        string diagnostics = Path.Combine(scratch, "diagnostics.txt");
        string assembly = System.Reflection.Assembly.GetExecutingAssembly().Location;
        Equal(4, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(assembly, diagnostics));
        var rows = File.ReadAllLines(Path.Combine(scratch, "source-runtime-values.txt"));
        Equal("BUNNY_MANAGED_DEFAULTS_V2", rows[0]);
        var declared = rows.Skip(1).OrderBy(s => s, StringComparer.Ordinal).ToArray();
        Equal(4, declared.Length);
        Equal("RARITY\\tFailingRarity".Replace("\\\\t", "\\t"), declared[0]);
        Equal("RARITY\\tFallingRarity".Replace("\\\\t", "\\t"), declared[1]);
        Equal("RARITY\\tFixedRarity".Replace("\\\\t", "\\t"), declared[2]);
        Equal("RARITY\\tRisingRarity".Replace("\\\\t", "\\t"), declared[3]);
        Equal(true, File.ReadAllText(diagnostics).Contains("ASSEMBLY_EXEC_OK", StringComparison.Ordinal));
    }
    finally
    {
        Directory.Delete(scratch, recursive: true);
    }
});

Console.WriteLine("RarityLoader host tests: passed=" + passed + " failed=" + failed);
return failed == 0 ? 0 : 1;

sealed class RisingRarity : BunnyRarity
{
    public int Red = 40;
    public int SetupCalls;
    public int HigherId = -1;
    public override BunnyColor RarityColor => new((byte)Red, 210, 240);
    public override void SetStaticDefaults() => SetupCalls++;
    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset > 0 && HigherId >= 0 ? HigherId : Type;
}

sealed class FallingRarity : BunnyRarity
{
    public int LowerId = -1;
    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset < 0 && LowerId >= 0 ? LowerId : Type;
}

sealed class FixedRarity : BunnyRarity
{
    public int Result;
    public override int GetPrefixedRarity(int offset, float valueMultiplier) => Result;
}

sealed class FailingRarity : BunnyRarity
{
    public override void SetStaticDefaults() => throw new InvalidOperationException("Expected setup failure");
}
