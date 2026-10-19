using BunnyLoader.SourceCompiler;
using System.Globalization;

if (args.Length != 1 || !File.Exists(args[0]))
    throw new InvalidOperationException("Expected compiled C# prefix fixture path");

string temp = Path.Combine(Path.GetTempPath(), "bunny-prefix-v89-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
    string diagnostics = Path.Combine(temp, "diagnostics.txt");
    File.WriteAllText(diagnostics, "");
    int classes = CompiledAssemblyProbe.Verify(args[0], diagnostics);
    if (classes != 5)
        throw new InvalidOperationException("Expected two content types and three managed prefixes, got " + classes);

    var data = File.ReadAllLines(Path.Combine(temp, "source-runtime-values.txt"));
    if (data.Length != 8 || data[0] != "BUNNY_MANAGED_DEFAULTS_V2")
        throw new InvalidOperationException("Unexpected MonoVM native handoff");

    if (data.Count(row => row == "PREFIX_CHANCE\tTestWand\t0") != 1)
        throw new InvalidOperationException("Managed item PrefixChance policy missing");
    if (data.Count(row => row == "PREFIX_APPLY\tTestWand\t4\t125") != 1)
        throw new InvalidOperationException("Managed item ApplyPrefix policy missing");
    var byName = data.Skip(1).Where(row => !row.StartsWith("PREFIX_CHANCE\t", StringComparison.Ordinal) &&
                                          !row.StartsWith("PREFIX_APPLY\t", StringComparison.Ordinal))
        .Select(row => row.Split('\t'))
        .ToDictionary(fields => fields[1], StringComparer.Ordinal);
    if (byName.Count != 5 || !byName.ContainsKey("TestBolt") || !byName.ContainsKey("TestWand"))
        throw new InvalidOperationException("Source content lost in prefix handoff");

    void Check(string name, int category, float weight, float damage, float value, int defense)
    {
        string[] fields = byName[name];
        int expectedLength = name == "TestMightyPrefix" || name == "TestCustomPrefix" ? 17 : 16;
        if (fields.Length != expectedLength || fields[0] != "PREFIX" ||
            int.Parse(fields[2], CultureInfo.InvariantCulture) != category ||
            MathF.Abs(float.Parse(fields[3], CultureInfo.InvariantCulture) - weight) > .0001f ||
            MathF.Abs(float.Parse(fields[4], CultureInfo.InvariantCulture) - damage) > .0001f ||
            MathF.Abs(float.Parse(fields[13], CultureInfo.InvariantCulture) - value) > .0001f ||
            int.Parse(fields[14], CultureInfo.InvariantCulture) != defense ||
            string.IsNullOrWhiteSpace(fields[15]))
            throw new InvalidOperationException("Managed prefix does not match expected descriptor: " + name);
    }

    Check("TestMightyPrefix", 4, 5f, 1.2f, 1.26f, 0);
    if (byName["TestMightyPrefix"][16] != "1:3:0,2:0:1,512:7,TestWand:2")
        throw new InvalidOperationException("Managed prefix per-item rules are not canonical");
    Check("TestGuardedPrefix", 5, 1f, 1f, 1.2f, 4);
    Check("TestCustomPrefix", 6, 1f, 1f, 1f, 0);
    if (byName["TestCustomPrefix"][16] != "1:0,TestWand:0:1:1")
        throw new InvalidOperationException("Custom prefix must keep explicit item rules only");
    var rules = new BunnyLoader.Source.BunnyPrefixRollRules();
    rules.Deny(12);
    rules.Allow(12, 5);
    if (rules.Serialize() != "12:5")
        throw new InvalidOperationException("Managed prefix item rule replace failed");
    bool invalidRejected = false;
    try { rules.SetWeight(0, 1); }
    catch (ArgumentOutOfRangeException) { invalidRejected = true; }
    if (!invalidRejected)
        throw new InvalidOperationException("Invalid item type accepted");
    bool prematureRejected = false;
    try { rules.SetCanRoll(18, true); }
    catch (InvalidOperationException) { prematureRejected = true; }
    if (!prematureRejected)
        throw new InvalidOperationException("CanRoll override without item rule accepted");
    bool preferenceRejected = false;
    try { rules.PreferFor(12); }
    catch (InvalidOperationException) { preferenceRejected = true; }
    if (preferenceRejected)
        throw new InvalidOperationException("Positive weight should allow a preferred prefix");
    if (rules.Serialize() != "12:5:1:1")
        throw new InvalidOperationException("Preferred prefix wire missing");
    rules.Deny(12);
    if (rules.Serialize() != "12:0")
        throw new InvalidOperationException("Deny must cancel preferred prefix");
    bool disallowedPreferenceRejected = false;
    try { rules.PreferFor(12); }
    catch (InvalidOperationException) { disallowedPreferenceRejected = true; }
    if (!disallowedPreferenceRejected)
        throw new InvalidOperationException("Disallowed preferred item accepted");
    var named = new BunnyLoader.Source.BunnyPrefixRollRules();
    named.Deny("ExampleWand");
    named.Allow("ExampleWand", 6);
    named.SetCanRoll("ExampleWand", false);
    if (named.Serialize() != "ExampleWand:6:0")
        throw new InvalidOperationException("Named item eligibility override failed");
    named.Deny("ExampleWand");
    if (named.Serialize() != "ExampleWand:0")
        throw new InvalidOperationException("Deny must clear explicit eligibility");
    bool invalidNameRejected = false;
    try { named.Deny("examplemod/ExampleWand"); }
    catch (ArgumentOutOfRangeException) { invalidNameRejected = true; }
    if (!invalidNameRejected)
        throw new InvalidOperationException("Unsafe stable item name accepted");
    Console.WriteLine("BUNNY_CSHARP_PREFIX_NATIVE_CONTRACT=PASS prefixes=3 callbacks=5 content=2");
}
finally
{
    Directory.Delete(temp, true);
}
