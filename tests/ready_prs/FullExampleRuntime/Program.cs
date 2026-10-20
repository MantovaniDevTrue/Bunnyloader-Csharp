using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;

if (args.Length != 1)
    throw new ArgumentException("Usage: FullExampleRuntime <ExampleMod playable root>");

string root = Path.GetFullPath(args[0]);
string work = Path.Combine(Path.GetTempPath(), "bunny-full-example-" + Guid.NewGuid().ToString("N"));
string framework = Path.Combine(work, "framework");
string output = Path.Combine(work, "compiled", "FullExample.dll");
string diagnostics = Path.Combine(work, "diagnostics.txt");
Directory.CreateDirectory(framework);
Directory.CreateDirectory(Path.GetDirectoryName(output)!);
try
{
    string[] references = {
        "System.Private.CoreLib.dll", "System.Runtime.dll", "netstandard.dll",
        "System.Collections.dll", "System.Collections.Concurrent.dll", "System.Linq.dll",
        "System.Console.dll", "System.Runtime.Extensions.dll", "System.ObjectModel.dll",
        "System.Threading.dll", "System.Threading.Tasks.dll", "System.Numerics.Vectors.dll",
        "System.ComponentModel.Primitives.dll"
    };
    var trusted = ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")
        ?? throw new InvalidOperationException("Host reference list unavailable"))
        .Split(Path.PathSeparator)
        .GroupBy(path => Path.GetFileName(path), StringComparer.OrdinalIgnoreCase)
        .ToDictionary(group => group.Key, group => group.First(), StringComparer.OrdinalIgnoreCase);
    foreach (string name in references)
    {
        if (!trusted.TryGetValue(name, out string? location))
            throw new InvalidOperationException("Required reference missing: " + name);
        File.Copy(location, Path.Combine(framework, name));
    }
    File.Copy(typeof(BunnyItem).Assembly.Location, Path.Combine(framework, "BunnySourceSdk.dll"));

    int compiled = Compiler.Compile(root, output, framework, diagnostics);
    if (compiled != 1)
        throw new InvalidOperationException("Full ExampleMod Roslyn returned " + compiled +
            "\n" + File.ReadAllText(diagnostics));

    int verified = CompiledAssemblyProbe.Verify(output, diagnostics, root);
    if (verified < 50)
        throw new InvalidOperationException("Full ExampleMod managed validation returned " +
            verified + "\n" + File.ReadAllText(diagnostics));

    var defaults = File.ReadAllText(Path.Combine(work, "source-runtime-values.txt"));
    if (!defaults.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !defaults.Contains("RARITY\tExampleHigherTierModRarity\n", StringComparison.Ordinal) ||
        !defaults.Contains("ITEM\tExampleItem\t", StringComparison.Ordinal))
        throw new InvalidOperationException("Full ExampleMod native handoff is incomplete");

    Console.WriteLine("FULL_EXAMPLE_ROSLYN_AND_EXECUTION=PASS classes=" + verified);
}
finally
{
    Directory.Delete(work, true);
}
