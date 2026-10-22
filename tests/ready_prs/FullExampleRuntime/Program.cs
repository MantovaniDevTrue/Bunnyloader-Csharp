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
    // A mesma montagem que cria NPCs e projéteis também registra classes de
    // dano do assembly real, sem JS e sem registro manual no mod.
    DamageClass registeredDamageClass = DamageClassLoader.GetDamageClass(10)
        ?? throw new InvalidOperationException("ExampleDamageClass was not auto-registered");
    if (registeredDamageClass.GetType().FullName !=
            "ExampleModCSharp.DamageClasses.ExampleDamageClass" ||
        !registeredDamageClass.CountsAsClass(DamageClass.Melee) ||
        !registeredDamageClass.CountsAsClass(DamageClass.Magic) ||
        registeredDamageClass.CountsAsClass(DamageClass.Summon))
        throw new InvalidOperationException("Auto-registered ExampleDamageClass effects do not match");
    var classStats = new DamageClassPlayer();
    if (classStats.GetTotalCritChance(registeredDamageClass) != 8f ||
        classStats.GetTotalArmorPenetration(registeredDamageClass) != 10f)
        throw new InvalidOperationException("Auto-registered ExampleDamageClass defaults differ");
    if (!BunnyEntitySourceDispatcher.Status(entitySlot).Contains("damageClasses=1", StringComparison.Ordinal))
        throw new InvalidOperationException("Managed entity context missing DamageClass registration");
    if (BunnyEntitySourceDispatcher.Register(modId, output) != entitySlot ||
        DamageClassLoader.DamageClassCount != 11)
        throw new InvalidOperationException("Repeated registration changed class IDs or context");
    bool duplicateRejected = false;
    try { DamageClassLoader.RegisterAssembly(System.Reflection.Assembly.LoadFrom(output)); }
    catch (InvalidOperationException) { duplicateRejected = true; }
    if (!duplicateRejected || DamageClassLoader.DamageClassCount != 11 ||
        !ReferenceEquals(DamageClassLoader.GetDamageClass(10), registeredDamageClass))
        throw new InvalidOperationException("Duplicate assembly registration corrupted DamageClass registry");
    Console.WriteLine("FULL_EXAMPLE_DAMAGECLASS_AUTO_REGISTRATION=PASS id=10 crit=8 pen=10");
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
    // Revogar o mod não pode alterar os IDs vanilla nem deixar uma classe
    // personalizada registrada após o encerramento do assembly.
    if (!BunnyEntitySourceDispatcher.Unregister(modId) ||
        DamageClassLoader.GetDamageClass(10) != null ||
        DamageClassLoader.DamageClassCount != 11 ||
        DamageClassLoader.All.Count != 10 ||
        BunnyEntitySourceDispatcher.Register(modId, output) >= 0)
        throw new InvalidOperationException("ExampleMod DamageClass per-mod cleanup failed");
    Console.WriteLine("FULL_EXAMPLE_DAMAGECLASS_MOD_UNLOAD=PASS stableIds=true");
    string onlyRoot = Path.Combine(work, "damageclass-only-mod");
    string onlyContent = Path.Combine(onlyRoot, "content");
    Directory.CreateDirectory(onlyContent);
    File.WriteAllText(Path.Combine(onlyContent, "SoloDamage.cs"),
        "using BunnyLoader.Source; namespace OnlyDamage; " +
        "public sealed class SoloDamage : DamageClass { " +
        "public override StatInheritanceData GetModifierInheritance(DamageClass other) " +
        "=> ReferenceEquals(other, Generic) ? StatInheritanceData.Full : StatInheritanceData.None; }");
    string onlyOutput = Path.Combine(work, "compiled", "OnlyDamage.dll");
    string onlyDiagnostics = Path.Combine(work, "only-diagnostics.txt");
    if (Compiler.Compile(onlyRoot, onlyOutput, framework, onlyDiagnostics) != 1 ||
        CompiledAssemblyProbe.Verify(onlyOutput, onlyDiagnostics, onlyRoot) != 1)
        throw new InvalidOperationException("DamageClass-only mod must compile and verify: " +
            File.ReadAllText(onlyDiagnostics));
    int onlySlot = BunnyEntitySourceDispatcher.Register("damageclass-only-smoke", onlyOutput);
    DamageClass? onlyClass = DamageClassLoader.GetDamageClass(11);
    if (onlySlot < 0 || onlyClass?.GetType().FullName != "OnlyDamage.SoloDamage" ||
        !onlyClass.GetModifierInheritance(DamageClass.Generic).Equals(StatInheritanceData.Full))
        throw new InvalidOperationException("Managed-only DamageClass registration failed");
    if (!BunnyEntitySourceDispatcher.Unregister("damageclass-only-smoke") ||
        DamageClassLoader.GetDamageClass(11) != null ||
        DamageClassLoader.All.Count != 10)
        throw new InvalidOperationException("Managed-only DamageClass unload failed");
    Console.WriteLine("DAMAGECLASS_ONLY_ASSEMBLY_COMPILE_AND_UNLOAD=PASS");


}
finally
{
    Directory.Delete(work, true);
}
