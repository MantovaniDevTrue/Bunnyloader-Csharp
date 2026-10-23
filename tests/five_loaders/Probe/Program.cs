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
    if (verified != 5)
        throw new InvalidOperationException("Full ExampleMod managed validation returned " +
            verified + "\n" + File.ReadAllText(diagnostics));

    string wire = File.ReadAllText(Path.Combine(work, "source-runtime-values.txt"));
    if (!wire.Contains("WEAPON\tQaWand\t40\t", StringComparison.Ordinal))
        throw new InvalidOperationException("StatModifier computed damage did not reach native defaults\n" + wire);
    var assembly = System.Reflection.Assembly.LoadFrom(output);
    var world = assembly.GetType("FiveLoadersCSharp.QaWorld", true)!;
    var dispatcher = new BunnyWorldDispatcher();
    const string uid = "fd8b8d75-62c9-4ae9-9000-45d9be591501";
    if (!dispatcher.TryRegisterAssembly(uid, assembly, out string error) || dispatcher.RegisteredSystems != 1)
        throw new InvalidOperationException(error);
    if (!dispatcher.TryEnterWorld(1, null, true, out error)) throw new InvalidOperationException(error);
    world.GetMethod("AddPress")!.Invoke(null, null);
    world.GetMethod("AddPress")!.Invoke(null, null);
    if (!dispatcher.TryPrepareSave(1, out byte[] saved, out error)) throw new InvalidOperationException(error);
    if (!BunnyWorldDocument.TryDecode(saved, out var document) ||
        document.Read(uid, "FiveLoadersCSharp.QaWorld").GetInt32("presses") != 2)
        throw new InvalidOperationException("QA example did not save the two presses");
    if (!dispatcher.TryConfirmSaved(1, saved)) throw new InvalidOperationException("Save confirmation");
    if (!dispatcher.TryLeaveWorld(1, out error)) throw new InvalidOperationException(error);
    if (!dispatcher.TryEnterWorld(2, saved, true, out error)) throw new InvalidOperationException(error);
    if ((int)world.GetField("SavedPresses")!.GetValue(null)! != 2)
        throw new InvalidOperationException("QA example failed restore");
    if (!dispatcher.TryLeaveWorld(2, out error)) throw new InvalidOperationException(error);
    if (!dispatcher.TryEnterWorld(3, null, true, out error)) throw new InvalidOperationException(error);
    if ((int)world.GetField("SavedPresses")!.GetValue(null)! != 0)
        throw new InvalidOperationException("QA example leaked state to a new world");
    if (!dispatcher.TryLeaveWorld(3, out error)) throw new InvalidOperationException(error);
    Console.WriteLine("FIVE_QA_ROSLYN_DEFAULTS_WORLD_ROUNDTRIP_PASS");
}
finally { Directory.Delete(work, true); }
