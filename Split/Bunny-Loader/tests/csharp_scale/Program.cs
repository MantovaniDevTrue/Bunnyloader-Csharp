using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using System.Diagnostics;
using System.Reflection;
using System.Text.Json;

int[] sizes = args.Length == 0 ? new[] { 100, 300, 600, 1000 } :
    args.Select(int.Parse).ToArray();
if (sizes.Any(n => n < 1 || n > 4096))
    throw new ArgumentOutOfRangeException(nameof(args));
string temporary = Path.Combine(Path.GetTempPath(), "bunny-scale-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temporary);
try
{
    string runtime = Path.Combine(temporary, "runtime");
    Directory.CreateDirectory(runtime);
    string framework = Path.GetDirectoryName(typeof(object).Assembly.Location)!;
    foreach (string name in new[] {
        "System.Private.CoreLib", "System.Runtime", "netstandard", "System.Collections",
        "System.Collections.Concurrent", "System.Linq", "System.Console", "System.Runtime.Extensions",
        "System.ObjectModel", "System.Threading", "System.Threading.Tasks", "System.Numerics.Vectors",
        "System.ComponentModel.Primitives"
    })
        File.Copy(Path.Combine(framework, name + ".dll"), Path.Combine(runtime, name + ".dll"));
    File.Copy(typeof(BunnyItem).Assembly.Location, Path.Combine(runtime, "BunnySourceSdk.dll"));
    var result = new List<object>();
    foreach (int count in sizes)
    {
        string folder = Path.Combine(temporary, "fixture-" + count);
        string content = Path.Combine(folder, "content");
        Directory.CreateDirectory(content);
        for (int i = 0; i < count; i++)
        {
            string typeName = $"ScaleItem{i:D4}";
            File.WriteAllText(Path.Combine(content, typeName + ".cs"),
                "using BunnyLoader.Source;\n" +
                "namespace BunnyScaleFixture;\n" +
                "public sealed class " + typeName + " : BunnyItem {\n" +
                "    public override void SetDefaults() {\n" +
                "        DisplayName = \"Scale Item\";\n" +
                "        Texture = \"Content/Items/Scale.png\";\n" +
                "        MaxStack = 9999;\n" +
                "        Value = 100;\n" +
                "    }\n" +
                "}\n");
        }
        string output = Path.Combine(temporary, $"scale-{count}.dll");
        string diagnostic = Path.Combine(temporary, $"scale-{count}.txt");
        GC.Collect();
        GC.WaitForPendingFinalizers();
        var watch = Stopwatch.StartNew();
        int compiled = Compiler.Compile(folder, output, runtime, diagnostic);
        long compileMs = watch.ElapsedMilliseconds;
        string compileDetails = File.Exists(diagnostic) ? File.ReadAllText(diagnostic) : "";
        if (compiled != 1) throw new Exception($"CSharp scale compile {count}: {compileDetails}");
        watch.Restart();
        int verified = CompiledAssemblyProbe.Verify(output, diagnostic);
        long defaultsMs = watch.ElapsedMilliseconds;
        if (verified != count)
            throw new Exception($"CSharp scale managed defaults {count} actual={verified}: " +
                File.ReadAllText(diagnostic));
        watch.Restart();
        int cache = Compiler.Compile(folder, output, runtime, diagnostic);
        long cacheMs = watch.ElapsedMilliseconds;
        if (cache != 2) throw new Exception($"CSharp scale verified cache failed count={count}");
        int exported = File.ReadLines(Path.Combine(temporary, "source-runtime-values.txt"))
            .Count(line => line.StartsWith("ITEM\tScaleItem", StringComparison.Ordinal));
        if (exported != count)
            throw new Exception($"CSharp scale native wire expected={count} actual={exported}");
        var row = new {
            classes = count,
            sourceFiles = count,
            coldCompileMs = compileMs,
            managedDefaultsMs = defaultsMs,
            warmCompileMs = cacheMs,
            assemblyBytes = new FileInfo(output).Length,
            committedMemoryBytes = GC.GetGCMemoryInfo().TotalCommittedBytes,
            compiler = compileDetails.Trim()
        };
        result.Add(row);
        Console.WriteLine(JsonSerializer.Serialize(row));
    }
    string overflowContent = Path.Combine(temporary, "over-limit", "content");
    Directory.CreateDirectory(overflowContent);
    for (int i = 0; i < 4097; i++)
        File.WriteAllText(Path.Combine(overflowContent, $"Source{i:D4}.cs"), "public sealed class EmptySource {}");
    string overflowRoot = Path.GetDirectoryName(overflowContent)!;
    string overflowDiagnostics = Path.Combine(temporary, "over-limit.txt");
    int overLimitResult = Compiler.Compile(overflowRoot,
        Path.Combine(temporary, "over-limit.dll"), runtime, overflowDiagnostics);
    if (overLimitResult != -3 || !File.ReadAllText(overflowDiagnostics).Contains("permitido=1..4096"))
        throw new Exception("The 4096 file safety ceiling was not enforced");
    Console.WriteLine("BUNNY_CSHARP_4097_FILES_SAFETY_REJECTED=PASS");
    Console.WriteLine("BUNNY_CSHARP_SCALE_ROSLYN_VERIFIED=PASS");
}
finally
{
    Directory.Delete(temporary, recursive: true);
}
