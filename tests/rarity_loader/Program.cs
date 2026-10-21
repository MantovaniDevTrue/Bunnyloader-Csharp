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

Check("ids.host-assigned-offset-for-multiple-mods", () =>
{
    var first = new BunnyRarityRegistry();
    var second = new BunnyRarityRegistry(20);
    Equal(12, first.Register("mod-a", new RisingRarity()));
    Equal(20, second.Register("mod-b", new FallingRarity()));
    Equal(21, second.RarityCount);
    second.Seal();
    Equal(false, second.TryGetColor(12, out _));
    Equal(true, second.TryGetColor(20, out _));
    Equal(20, second.ResolvePrefixedRarity(20, 1.0f, 11));
    Throws<ArgumentOutOfRangeException>(() => new BunnyRarityRegistry(11));
    Throws<ArgumentOutOfRangeException>(() => new BunnyRarityRegistry(4110));
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
        Equal("RARITY\tFailingRarity", declared[0]);
        Equal("RARITY\tFallingRarity", declared[1]);
        Equal("RARITY\tFixedRarity", declared[2]);
        Equal("RARITY\tRisingRarity", declared[3]);
        Equal(true, File.ReadAllText(diagnostics).Contains("ASSEMBLY_EXEC_OK", StringComparison.Ordinal));
    }
    finally
    {
        Directory.Delete(scratch, recursive: true);
    }
});

Check("runtime.original-example-assembly-wire", () =>
{
    string assembly = typeof(ExampleModCSharp.Rarities.ExampleModRarity).Assembly.Location;
    string scratch = Path.Combine(Path.GetTempPath(), "bunny-original-rarity-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(scratch);
    try
    {
        string diagnostic = Path.Combine(scratch, "diagnostics.txt");
        Equal(8, BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(assembly, diagnostic));
        var descriptors = File.ReadAllLines(Path.Combine(scratch, "source-runtime-values.txt"));
        Equal("BUNNY_MANAGED_DEFAULTS_V2", descriptors[0]);
        Equal(true, descriptors.Any(s => s.StartsWith("ITEM\tExampleRarityItem\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Any(s => s.StartsWith("ACCESSORY\tExampleRarityAccessory\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Contains("RARITY\tExampleHigherTierModRarity"));
        Equal(true, descriptors.Contains("RARITY\tExampleModRarity"));
        Equal(true, descriptors.Contains("ITEM_RARITY\tExampleRarityItem\tExampleModRarity"));
        Equal(true, descriptors.Contains("ITEM_RARITY\tExampleRarityAccessory\tExampleHigherTierModRarity"));
        Equal(true, descriptors.Any(s => s.StartsWith("WEAPON\tExampleRarityWeapon\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Any(s => s.StartsWith("POTION\tExampleRarityPotion\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Any(s => s.StartsWith("PROJECTILE\tExampleRarityProjectile\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Any(s => s.StartsWith("BUFF\tExampleRarityBuff\t", StringComparison.Ordinal)));
        Equal(true, descriptors.Contains("ITEM_RARITY\tExampleRarityWeapon\tExampleModRarity"));
        Equal(true, descriptors.Contains("ITEM_RARITY\tExampleRarityPotion\tExampleHigherTierModRarity"));
    }
    finally
    {
        Directory.Delete(scratch, recursive: true);
    }
});

Check("item.typed-rarity-uses-owner-identity-not-runtime-integer", () =>
{
    var item = new ExampleModCSharp.Rarities.ExampleRarityItem();
    item.SetDefaults();
    Equal(0, item.Rare);
    Equal("ExampleModRarity", item.ModRarityClassName);
    var second = new ExampleModCSharp.Rarities.ExampleRarityItem();
    second.SetRarity<ExampleModCSharp.Rarities.ExampleHigherTierModRarity>();
    Equal("ExampleHigherTierModRarity", second.ModRarityClassName);
    Throws<ArgumentException>(() => second.SetRarity<RisingRarity>());
    var accessory = new ExampleModCSharp.Rarities.ExampleRarityAccessory();
    accessory.SetDefaults();
    Equal("ExampleHigherTierModRarity", accessory.ModRarityClassName);
    Throws<ArgumentException>(() => accessory.SetRarity<RisingRarity>());
    var weapon = new ExampleModCSharp.Rarities.ExampleRarityWeapon();
    weapon.SetDefaults();
    Equal("ExampleModRarity", weapon.ModRarityClassName);
    Throws<ArgumentException>(() => weapon.SetRarity<RisingRarity>());
    var potion = new ExampleModCSharp.Rarities.ExampleRarityPotion();
    potion.SetDefaults();
    Equal("ExampleHigherTierModRarity", potion.ModRarityClassName);
    Throws<ArgumentException>(() => potion.SetRarity<RisingRarity>());
});

Check("runtime.original-example-colors-prefixes-and-full-bind", () =>
{
    string assembly = typeof(ExampleModCSharp.Rarities.ExampleModRarity).Assembly.Location;
    Equal("ERROR|Stage=ResolveRarity Reason=OwnerNotBound",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 0, "200,125,150"));
    Equal("ERROR|Stage=BindRarity Reason=ClassCountMismatch",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 2, "ExampleModRarity:14"));
    Equal("ERROR|Stage=BindRarity Reason=AmbiguousClassOrId",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 2,
            "ExampleModRarity:14;ExampleHigherTierModRarity:14"));
    Equal("READY|2", BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 2,
        "ExampleModRarity:14;ExampleHigherTierModRarity:15"));
    Equal(14, BunnyRarityRuntime.TypeOf<ExampleModCSharp.Rarities.ExampleModRarity>("examplemodcsharp"));
    Equal(15, BunnyRarityRuntime.TypeOf<ExampleModCSharp.Rarities.ExampleHigherTierModRarity>("examplemodcsharp"));
    Equal("COLOR|200|215|230|255",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 0, "200,125,150"));
    Equal("COLOR|100|100|100|255",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleHigherTierModRarity", 0, 0, "200,125,150"));
    Equal("COLOR|40|160|20|255",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleHigherTierModRarity", 0, 0, "80,200,30"));
    Equal("COLOR|100|100|100|255",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleHigherTierModRarity", 0, 0, "200,125,150"));
    Equal("RARITY|15",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 1, "1.2,11"));
    Equal("RARITY|14",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleHigherTierModRarity", 0, 1, "0.8,11"));
    Equal("RARITY|14",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 1, "1,11"));
    Equal("ERROR|Stage=PrefixRarity Reason=InvalidPrefixData",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 1, "NaN,11"));
    Equal("ERROR|Stage=ReadRarityColor Reason=InvalidDiscoChannels",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "ExampleModRarity", 0, 0, "300,0,0"));
    Equal("ERROR|Stage=BindRarity Reason=EmptyOrAlreadyBound",
        BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 2,
            "ExampleModRarity:14;ExampleHigherTierModRarity:15"));
    Equal("CLEARED", BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 3, ""));
    Throws<KeyNotFoundException>(() =>
        BunnyRarityRuntime.TypeOf<ExampleModCSharp.Rarities.ExampleModRarity>("examplemodcsharp"));
    Equal("READY|2", BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 2,
        "ExampleModRarity:18;ExampleHigherTierModRarity:19"));
    Equal(19, BunnyRarityRuntime.TypeOf<ExampleModCSharp.Rarities.ExampleHigherTierModRarity>("examplemodcsharp"));
    Equal("CLEARED", BunnyRarityRuntime.Dispatch("examplemodcsharp", assembly, "", 0, 3, ""));
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
