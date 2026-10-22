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

    string modId = "full-example-dust-validation";
    int entitySlot = BunnyEntitySourceDispatcher.Register(modId, output);
    if (entitySlot < 0)
        throw new InvalidOperationException("Full Example entity registration failed: " +
            BunnyEntitySourceDispatcher.LastRegistrationError);
    var dustOwner = typeof(BunnyDustSourceDispatcher).GetMethod("ModForAssembly",
        System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)
        ?? throw new InvalidOperationException("BunnyDust owner lookup missing");
    var exampleAssembly = System.Reflection.Assembly.LoadFrom(output);
    var dustClass = exampleAssembly.GetTypes().Single(t => t.Name == "ExampleSparkDust");
    string dustMod;
    try
    {
        dustMod = (string)(dustOwner.Invoke(null, new object[] { dustClass.Assembly })
            ?? throw new InvalidOperationException("BunnyDust owner is null"));
    }
    catch (System.Reflection.TargetInvocationException error)
    {
        throw new InvalidOperationException(
            "Full Example BunnyDust ownership failed. EntityStatus=" +
            BunnyEntitySourceDispatcher.Status(entitySlot) + " DustRegister=" +
            BunnyDustSourceDispatcher.LastRegistrationError, error.InnerException ?? error);
    }
    if (dustMod != modId)
        throw new InvalidOperationException("BunnyDust resolved wrong owner: " + dustMod);
    BunnyDustSourceDispatcher.Unload();
    string restored = (string)(dustOwner.Invoke(null, new object[] { dustClass.Assembly })
        ?? throw new InvalidOperationException("BunnyDust recovery returned null"));
    if (restored != modId)
        throw new InvalidOperationException("Trusted assembly Dust retry failed");
    bool untrustedFailed = false;
    try
    {
        dustOwner.Invoke(null, new object[] { typeof(BunnyDust).Assembly });
    }
    catch (System.Reflection.TargetInvocationException error)
        when (error.InnerException is InvalidOperationException)
    {
        untrustedFailed = true;
    }
    if (!untrustedFailed)
        throw new InvalidOperationException("Unregistered assemblies cannot claim a Dust mod");
    Console.WriteLine("FULL_EXAMPLE_DUST_ASSEMBLY_OWNERSHIP=PASS");

    var defaults = File.ReadAllText(Path.Combine(work, "source-runtime-values.txt"));
    if (!defaults.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        !defaults.Contains("RARITY\tExampleHigherTierModRarity\n", StringComparison.Ordinal) ||
        !defaults.Contains("ITEM\tExampleItem\t", StringComparison.Ordinal))
        throw new InvalidOperationException("Full ExampleMod native handoff is incomplete");

    string inventoryDirectory = Path.Combine(Directory.GetCurrentDirectory(), "build", "ready-pr-tests");
    Directory.CreateDirectory(inventoryDirectory);
    File.WriteAllText(Path.Combine(inventoryDirectory, "managed-class-inventory.txt"),
        "executedTypes=" + verified + "\nhookModules=" + CompiledAssemblyProbe.LastVerifiedHookModules + "\n");
    Console.WriteLine("FULL_EXAMPLE_ROSLYN_AND_EXECUTION=PASS classes=" + verified);
    string compact = Path.Combine(framework, "compiler-references");
    File.WriteAllText(Path.Combine(framework, "framework-version.txt"), new string('a', 64));
    File.WriteAllText(Path.Combine(framework, "compiler-references.approved"), new string('a', 64));
    Directory.CreateDirectory(compact);
    foreach (string name in references)
    {
        if (name == "System.Private.CoreLib.dll") continue;
        File.Copy(Path.Combine(framework, name), Path.Combine(compact, name));
    }
    File.Copy(Path.Combine(framework, "BunnySourceSdk.dll"),
        Path.Combine(compact, "BunnySourceSdk.dll"));
    string compactOutput = Path.Combine(work, "compiled", "FullExampleCompact.dll");
    string compactDiagnostics = Path.Combine(work, "compact-diagnostics.txt");
    int compactCompiled = Compiler.Compile(root, compactOutput, framework, compactDiagnostics);
    if (compactCompiled != 1)
        throw new InvalidOperationException("Android-style compact Roslyn returned " + compactCompiled +
            "\n" + File.ReadAllText(compactDiagnostics));
    int compactVerified = CompiledAssemblyProbe.Verify(compactOutput, compactDiagnostics, root);
    if (compactVerified != verified)
        throw new InvalidOperationException("Android-style compact DLL validation returned " +
            compactVerified + "\n" + File.ReadAllText(compactDiagnostics));
    Console.WriteLine("FULL_EXAMPLE_ANDROID_COMPACT_REFERENCES=PASS classes=" + compactVerified);
    File.Delete(Path.Combine(framework, "compiler-references.approved"));
    string staleOutput = Path.Combine(work, "compiled", "FullExampleWithStaleCompact.dll");
    string staleDiagnostics = Path.Combine(work, "stale-diagnostics.txt");
    int staleCompiled = Compiler.Compile(root, staleOutput, framework, staleDiagnostics);
    if (staleCompiled != 1)
        throw new InvalidOperationException("Stale compact pack was not ignored: " +
            staleCompiled + "\n" + File.ReadAllText(staleDiagnostics));
    int staleVerified = CompiledAssemblyProbe.Verify(staleOutput, staleDiagnostics, root);
    if (staleVerified != verified || !File.ReadAllText(staleDiagnostics + ".profile")
        .Contains("referenceMode=Runtime", StringComparison.Ordinal))
        throw new InvalidOperationException("Runtime references not selected after stale-pack invalidation");
    Console.WriteLine("FULL_EXAMPLE_STALE_COMPACT_FALLBACK=PASS classes=" + staleVerified);
    string packaged = Path.Combine(work, "apk-android-runtime");
    Directory.CreateDirectory(packaged);
    string repoAssets = Path.GetFullPath("app/src/main/assets/managed");
    foreach (string name in references)
    {
        string from = Path.Combine(repoAssets, name);
        if (!File.Exists(from))
            throw new InvalidOperationException("Android packaged CLR reference missing: " + name);
        File.Copy(from, Path.Combine(packaged, name));
    }
    File.Copy(typeof(BunnyItem).Assembly.Location, Path.Combine(packaged, "BunnySourceSdk.dll"));
    string packagedOutput = Path.Combine(work, "compiled", "FullExamplePackagedAndroid.dll");
    string packagedDiagnostics = Path.Combine(work, "packaged-diagnostics.txt");
    int packagedCompiled = Compiler.Compile(root, packagedOutput, packaged, packagedDiagnostics);
    if (packagedCompiled != 1)
        throw new InvalidOperationException("Android packaged CLR Roslyn returned " + packagedCompiled +
            "\n" + File.ReadAllText(packagedDiagnostics));
    int packagedVerified = CompiledAssemblyProbe.Verify(packagedOutput, packagedDiagnostics, root);
    if (packagedVerified != verified)
        throw new InvalidOperationException("Android packaged CLR validation returned " +
            packagedVerified + "\n" + File.ReadAllText(packagedDiagnostics));
    Console.WriteLine("FULL_EXAMPLE_PACKAGED_ANDROID_CLR=PASS classes=" + packagedVerified);
}
finally
{
    Directory.Delete(work, true);
}
