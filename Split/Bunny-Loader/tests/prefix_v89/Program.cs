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
    if (classes != 4)
        throw new InvalidOperationException("Expected two content types and two managed prefixes, got " + classes);

    var data = File.ReadAllLines(Path.Combine(temp, "source-runtime-values.txt"));
    if (data.Length != 5 || data[0] != "BUNNY_MANAGED_DEFAULTS_V2")
        throw new InvalidOperationException("Unexpected MonoVM native handoff");

    var byName = data.Skip(1).Select(row => row.Split('\t'))
        .ToDictionary(fields => fields[1], StringComparer.Ordinal);
    if (byName.Count != 4 || !byName.ContainsKey("TestBolt") || !byName.ContainsKey("TestWand"))
        throw new InvalidOperationException("Source content lost in prefix handoff");

    void Check(string name, int category, float weight, float damage, float value, int defense)
    {
        string[] fields = byName[name];
        if (fields.Length != 16 || fields[0] != "PREFIX" ||
            int.Parse(fields[2], CultureInfo.InvariantCulture) != category ||
            MathF.Abs(float.Parse(fields[3], CultureInfo.InvariantCulture) - weight) > .0001f ||
            MathF.Abs(float.Parse(fields[4], CultureInfo.InvariantCulture) - damage) > .0001f ||
            MathF.Abs(float.Parse(fields[13], CultureInfo.InvariantCulture) - value) > .0001f ||
            int.Parse(fields[14], CultureInfo.InvariantCulture) != defense ||
            string.IsNullOrWhiteSpace(fields[15]))
            throw new InvalidOperationException("Managed prefix does not match expected descriptor: " + name);
    }

    Check("TestMightyPrefix", 4, 5f, 1.2f, 1.26f, 0);
    Check("TestGuardedPrefix", 5, 1f, 1f, 1.2f, 4);
    Console.WriteLine("BUNNY_CSHARP_PREFIX_NATIVE_CONTRACT=PASS prefixes=2 callbacks=4 content=2");
}
finally
{
    Directory.Delete(temp, true);
}
