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
    const string uid = "fd8b8d75-62c9-4ae9-9000-45d9be591501";
    if (!BunnyWorldMonoHost.RegisterMod(uid, output) || BunnyWorldMonoHost.RegisteredSystems != 1)
        throw new InvalidOperationException("World registration: " + BunnyWorldMonoHost.LastError);
    if (BunnyCommandWorker.RegisterAssembly(uid, output) != 1)
        throw new InvalidOperationException("Command registration failed");

    using var hooks = BunnyModAssemblyRegistry.Acquire(uid, output);
    using var secondHandle = BunnyModAssemblyRegistry.Acquire(uid, output);
    if (!ReferenceEquals(hooks.Assembly, secondHandle.Assembly) ||
        BunnyModAssemblyRegistry.ActiveModCount != 1)
        throw new InvalidOperationException("Different loaders received different mod assemblies");

    var assembly = hooks.Assembly;
    var world = assembly.GetType("FiveLoadersCSharp.QaWorld", true)!;
    var updates = assembly.GetType("FiveLoadersCSharp.QaUpdates", true)!;
    var counts = (long[])updates.GetField("Counts")!.GetValue(null)!;
    static string Chat()
    {
        string wire = BunnyCommandWorker.Execute("/lote", 0, 0, 1);
        return string.Join("\n", wire.Split('\n')
            .Where(line => line.StartsWith("REPLY\t", StringComparison.Ordinal))
            .Select(line => System.Text.Encoding.UTF8.GetString(
                Convert.FromHexString(line.Split('\t')[4]))));
    }

    if (!BunnyWorldMonoHost.Enter(1, null, true))
        throw new InvalidOperationException("World entry: " + BunnyWorldMonoHost.LastError);
    world.GetMethod("AddPress")!.Invoke(null, null);
    world.GetMethod("AddPress")!.Invoke(null, null);
    counts[0] = 1;
    string first = Chat();
    if (!first.Contains("mundo=True", StringComparison.Ordinal) ||
        !first.Contains("pontos=2", StringComparison.Ordinal) ||
        !first.Contains("updates=1/19", StringComparison.Ordinal))
        throw new InvalidOperationException("Command sees a different assembly than world/hooks:\n" + first);

    string saved64 = BunnyWorldMonoHost.Prepare(1)
        ?? throw new InvalidOperationException("World save: " + BunnyWorldMonoHost.LastError);
    byte[] saved = Convert.FromBase64String(saved64);
    if (!BunnyWorldDocument.TryDecode(saved, out var document) ||
        document.Read(uid, "FiveLoadersCSharp.QaWorld").GetInt32("presses") != 2)
        throw new InvalidOperationException("QA example did not save the two presses");
    if (!BunnyWorldMonoHost.Confirm(1, saved64))
        throw new InvalidOperationException("Save confirmation: " + BunnyWorldMonoHost.LastError);
    if (!BunnyWorldMonoHost.Leave(1))
        throw new InvalidOperationException("World exit: " + BunnyWorldMonoHost.LastError);
    if (!BunnyWorldMonoHost.Enter(2, saved64, true))
        throw new InvalidOperationException("World reload: " + BunnyWorldMonoHost.LastError);
    string restored = Chat();
    if (!restored.Contains("mundo=True", StringComparison.Ordinal) ||
        !restored.Contains("pontos=2", StringComparison.Ordinal) ||
        !restored.Contains("saves=1", StringComparison.Ordinal) ||
        !restored.Contains("updates=0/19", StringComparison.Ordinal))
        throw new InvalidOperationException("Command did not observe restored and reset state:\n" + restored);
    if (!BunnyWorldMonoHost.Leave(2))
        throw new InvalidOperationException("Second world exit: " + BunnyWorldMonoHost.LastError);
    if (!BunnyWorldMonoHost.Enter(3, null, true))
        throw new InvalidOperationException("New world entry: " + BunnyWorldMonoHost.LastError);
    if ((int)world.GetField("SavedPresses")!.GetValue(null)! != 0 ||
        !Chat().Contains("pontos=0", StringComparison.Ordinal))
        throw new InvalidOperationException("QA example leaked state to a new world");
    if (!BunnyWorldMonoHost.Leave(3))
        throw new InvalidOperationException("Third world exit: " + BunnyWorldMonoHost.LastError);

    string otherPath = Path.Combine(work, "compiled", "other.dll");
    File.Copy(output, otherPath);
    using (var other = BunnyModAssemblyRegistry.Acquire("qa-independent-mod", otherPath))
    {
        if (ReferenceEquals(other.Assembly, hooks.Assembly) ||
            BunnyModAssemblyRegistry.ActiveModCount != 2)
            throw new InvalidOperationException("Different mods must retain isolated assemblies");
    }
    bool rejected = false;
    try { using var wrongVersion = BunnyModAssemblyRegistry.Acquire(uid, otherPath); }
    catch (InvalidOperationException) { rejected = true; }
    if (!rejected) throw new InvalidOperationException("Conflicting mod assembly path was accepted");

    if (!BunnyWorldMonoHost.ResetRegistrations())
        throw new InvalidOperationException("World registrations were not reset");
    BunnyCommandWorker.Unregister(uid);
    secondHandle.Dispose();
    hooks.Dispose();
    if (BunnyModAssemblyRegistry.ActiveModCount != 0)
        throw new InvalidOperationException("Assembly references leaked after unload");
    using (var fresh = BunnyModAssemblyRegistry.Acquire(uid, output))
        if (ReferenceEquals(fresh.Assembly, assembly))
            throw new InvalidOperationException("Mod DLL remained cached after all owners left");
    if (BunnyModAssemblyRegistry.ActiveModCount != 0)
        throw new InvalidOperationException("Freshly loaded assembly was not released");
    Console.WriteLine("FIVE_QA_SHARED_ASSEMBLY_WORLD_COMMAND_HOOK_PASS");
    Console.WriteLine("FIVE_QA_ROSLYN_DEFAULTS_WORLD_ROUNDTRIP_PASS");
}
finally { Directory.Delete(work, true); }
