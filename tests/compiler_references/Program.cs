using BunnyLoader.SourceCompiler;
using System.Diagnostics;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text.Json;

if (args.Length != 4) throw new ArgumentException("mode sampleRoot runtimeDir resultsDir");
string mode = args[0], sample = Path.GetFullPath(args[1]), runtime = Path.GetFullPath(args[2]);
string results = Path.GetFullPath(args[3]); Directory.CreateDirectory(results);
string referenceRoot = Path.Combine(runtime, "compiler-references");
string hidden = referenceRoot + ".test-hidden";
if (mode == "runtime") Directory.Move(referenceRoot, hidden);
if (mode == "compact")
{
    const string identity = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    File.WriteAllText(Path.Combine(runtime, "framework-version.txt"), identity);
    File.WriteAllText(Path.Combine(runtime, "compiler-references.approved"), identity);
}
try
{
    string output = Path.Combine(results, mode + ".dll");
    string diagnostic = Path.Combine(results, mode + ".txt");
    var timer = Stopwatch.StartNew();
    int compiled = Compiler.Compile(sample, output, runtime, diagnostic);
    long coldMs = timer.ElapsedMilliseconds;
    string profile = File.ReadAllText(diagnostic + ".profile");
    if (compiled != 1 || !profile.Contains("referenceMode=" + (mode == "runtime" ? "Runtime" : "Compact")))
        throw new Exception("CompileContract " + profile);
    int types = CompiledAssemblyProbe.Verify(output, diagnostic);
    if (types < 1) throw new Exception(File.ReadAllText(diagnostic));
    foreach (string file in new[] { "source-runtime-values.txt", "source-projectile-turns.txt", "source-combat-policies.txt" })
        File.Copy(Path.Combine(results, file), Path.Combine(results, mode + "-" + file), true);
    if (Compiler.Compile(sample, output, runtime, diagnostic) != 2)
        throw new Exception("Assembly cache not reused");
    // Fresh output forces a real emit; metadata must be reused without keeping source trees per mod.
    if (Compiler.Compile(sample, output + ".second.dll", runtime, diagnostic) != 1 ||
        !File.ReadAllText(diagnostic + ".profile").Contains("metadataReused=1"))
        throw new Exception("Resident metadata snapshot not reused");
    if (mode == "compact")
    {
        foreach (string file in new[] { "source-runtime-values.txt", "source-projectile-turns.txt", "source-combat-policies.txt" })
            if (File.ReadAllText(Path.Combine(results, "runtime-" + file)) !=
                File.ReadAllText(Path.Combine(results, "compact-" + file)))
                throw new Exception("Runtime/compact behavior differs: " + file);
        string reference = Path.Combine(referenceRoot, "System.Runtime.dll");
        byte[] original = File.ReadAllBytes(reference);
        try
        {
            File.Delete(reference);
            if (Compiler.Compile(sample, output + ".missing.dll", runtime, diagnostic) != -7)
                throw new Exception("Partial reference pack accepted");
            File.WriteAllBytes(reference, new byte[] { 1, 2, 3 });
            if (Compiler.Compile(sample, output + ".corrupt.dll", runtime, diagnostic) > 0)
                throw new Exception("Changed reference reused stale metadata");
        }
        finally { File.WriteAllBytes(reference, original); }
        if (Compiler.Compile(sample, output + ".restored.dll", runtime, diagnostic) != 1)
            throw new Exception("Reference recovery failed");
    }
    long referenceBytes = 0, referenceMethods = 0, referenceTypes = 0;
    var referenceFiles = mode == "compact" ? Directory.GetFiles(referenceRoot, "*.dll") :
        Directory.GetFiles(runtime, "*.dll").Where(p => new[] { "System.Private.CoreLib.dll", "BunnySourceSdk.dll", "System.Runtime.dll", "netstandard.dll", "System.Collections.dll", "System.Collections.Concurrent.dll", "System.Linq.dll", "System.Console.dll", "System.Runtime.Extensions.dll", "System.ObjectModel.dll", "System.Threading.dll", "System.Threading.Tasks.dll", "System.Numerics.Vectors.dll", "System.ComponentModel.Primitives.dll" }.Contains(Path.GetFileName(p)));
    foreach (string file in referenceFiles)
    {
        referenceBytes += new FileInfo(file).Length;
        using var stream = File.OpenRead(file);
        using var pe = new PEReader(stream);
        var metadata = pe.GetMetadataReader();
        referenceTypes += metadata.TypeDefinitions.Count;
        referenceMethods += metadata.MethodDefinitions.Count;
    }
    var row = new { mode, coldMs, types, referenceBytes, referenceTypes, referenceMethods, compiler = profile };
    File.WriteAllText(Path.Combine(results, mode + "-benchmark.json"), JsonSerializer.Serialize(row));
    Console.WriteLine(JsonSerializer.Serialize(row));
    Console.WriteLine("FULL_EXAMPLEMOD_COMPACT_REFERENCE_BEHAVIOR_AND_METADATA_CACHE_PASS mode=" + mode);
}
finally { if (mode == "runtime") Directory.Move(hidden, referenceRoot); }
