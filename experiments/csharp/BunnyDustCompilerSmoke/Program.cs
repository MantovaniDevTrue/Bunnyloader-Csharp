using BunnyLoader.Source;
using BunnyLoader.SourceCompiler;
using System.Runtime.InteropServices;

string dir = Path.Combine(Path.GetTempPath(), "bunny-dust-smoke-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(dir);
try
{
    string referenceDirectory = Path.Combine(dir, "runtime");
    Directory.CreateDirectory(referenceDirectory);
    string systemDirectory = RuntimeEnvironment.GetRuntimeDirectory();
    string[] references =
    {
        "System.Private.CoreLib.dll", "System.Runtime.dll", "netstandard.dll",
        "System.Collections.dll", "System.Collections.Concurrent.dll",
        "System.Linq.dll", "System.Console.dll", "System.Runtime.Extensions.dll",
        "System.ObjectModel.dll", "System.Threading.dll",
        "System.Threading.Tasks.dll", "System.Numerics.Vectors.dll",
        "System.ComponentModel.Primitives.dll"
    };
    foreach (string name in references)
        File.Copy(Path.Combine(systemDirectory, name), Path.Combine(referenceDirectory, name));
    File.Copy(typeof(BunnyDust).Assembly.Location,
        Path.Combine(referenceDirectory, "BunnySourceSdk.dll"));
    string root = Path.Combine(dir, "example");
    Directory.CreateDirectory(Path.Combine(root, "content"));
    File.WriteAllText(Path.Combine(root, "content", "TestDust.cs"), """
using BunnyLoader.Source;
namespace BunnyDustSmoke;
public sealed class ExampleDust : BunnyDust
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Dusts/Example.png";
        UpdateType = 6;
    }
    public override void OnSpawn(ref BunnyDustState state)
    {
        state.Scale *= 1.2f;
    }
    public override bool MidUpdate(ref BunnyDustState state)
    {
        return true;
    }
}
public sealed class BrokenSpawnDust : BunnyDust
{
    public static int SpawnAttempts;
    public override void SetStaticDefaults() { }
    public override void OnSpawn(ref BunnyDustState state)
    {
        ++SpawnAttempts;
        throw new System.InvalidOperationException("Expected test failure");
    }
}
""");
    string output = Path.Combine(dir, "ExampleDust.dll");
    string diag = Path.Combine(dir, "compile.txt");
    int result = Compiler.Compile(root, output, referenceDirectory, diag);
    if (result != 1)
        throw new Exception("Compiler returned " + result + "\n" + File.ReadAllText(diag));
    int classes = CompiledAssemblyProbe.Verify(output, diag);
    if (classes != 2)
        throw new Exception("Probe returned " + classes + "\n" + File.ReadAllText(diag));
    string wire = File.ReadAllText(Path.Combine(dir, "source-runtime-values.txt"));
    if (!wire.Contains("DUST\tExampleDust\t6\t5\tContent/Dusts/Example.png\n",
        StringComparison.Ordinal) ||
        !wire.Contains("DUST\tBrokenSpawnDust\t-1\t1\t-\n", StringComparison.Ordinal) ||
        !wire.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        wire.Split('\n', StringSplitOptions.RemoveEmptyEntries).Length != 3)
        throw new Exception("Dust defaults wire mismatch:\n" + wire);
    int modSlot = BunnyDustSourceDispatcher.Register("smoke_dust", output);
    if (modSlot < 0)
        throw new Exception("Dust dispatcher register failed: " +
            BunnyDustSourceDispatcher.LastRegistrationError);
    if (!BunnyDustSourceDispatcher.Bind(modSlot, 332, "ExampleDust") ||
        BunnyDustSourceDispatcher.Bind(modSlot, 333, "ExampleDust") ||
        !BunnyDustSourceDispatcher.Bind(modSlot, 333, "BrokenSpawnDust"))
        throw new Exception("Dust dispatcher native binding failed");
    BunnyDustState state = new()
    {
        Slot = 15, Type = 332, Generation = 1, Active = 1,
        X = 5, Y = 8, Scale = 1, FrameWidth = 8, FrameHeight = 8
    };
    unsafe
    {
        int flags = BunnyDustSourceDispatcher.Tick(modSlot, (IntPtr)(&state));
        int expected = BunnyDustSourceDispatcher.Accepted |
            BunnyDustSourceDispatcher.VanillaUpdate |
            BunnyDustSourceDispatcher.KeepHorizontalSpeed |
            BunnyDustSourceDispatcher.Draw |
            BunnyDustSourceDispatcher.Active;
        if (flags != expected || Math.Abs(state.Scale - 1.2f) > 0.00001f)
            throw new Exception("Dust callback flags/OnSpawn mismatch");
        if (BunnyDustSourceDispatcher.Tick(modSlot, (IntPtr)(&state)) != expected ||
            Math.Abs(state.Scale - 1.2f) > 0.00001f)
            throw new Exception("Dust OnSpawn repeated in same instance");
        state.Generation++;
        if (BunnyDustSourceDispatcher.Tick(modSlot, (IntPtr)(&state)) != expected ||
            Math.Abs(state.Scale - 1.44f) > 0.00001f)
            throw new Exception("Dust generation did not spawn fresh");
        BunnyDustSourceDispatcher.ResetWorld();
        if (BunnyDustSourceDispatcher.Tick(modSlot, (IntPtr)(&state)) != expected ||
            Math.Abs(state.Scale - 1.728f) > 0.00001f)
            throw new Exception("Dust world reset lifecycle mismatch");
    }
    var broken = output;
    var brokenType = System.Reflection.Assembly.LoadFrom(broken)
        .GetType("BunnyDustSmoke.BrokenSpawnDust", throwOnError: true)!;
    var attempts = brokenType.GetField("SpawnAttempts")!;
    BunnyDustState failed = new()
    {
        Slot = 16, Type = 333, Generation = 1, Active = 1,
        X = 6, Y = 9, Scale = 1, FrameWidth = 8, FrameHeight = 8
    };
    unsafe
    {
        int first = BunnyDustSourceDispatcher.Invoke(modSlot,
            BunnyDustSourceDispatcher.Spawn, (IntPtr)(&failed));
        int second = BunnyDustSourceDispatcher.Invoke(modSlot,
            BunnyDustSourceDispatcher.Spawn, (IntPtr)(&failed));
        if ((first & BunnyDustSourceDispatcher.Accepted) == 0 ||
            (second & BunnyDustSourceDispatcher.Accepted) == 0 ||
            (int)attempts.GetValue(null)! != 1)
            throw new Exception("OnSpawn fault was repeatedly invoked on same slot/generation");
        failed.Generation = 2;
        BunnyDustSourceDispatcher.Invoke(modSlot,
            BunnyDustSourceDispatcher.Spawn, (IntPtr)(&failed));
        if ((int)attempts.GetValue(null)! != 2)
            throw new Exception("New Dust generation did not rearm quarantined callback");
    }
    BunnyDustSourceDispatcher.Unload();
    Console.WriteLine("DUST_SOURCE_ROSLYN_SMOKE=PASS");
    Console.WriteLine("DUST_MANAGED_MONOVM_DISPATCH_SMOKE=PASS");
}
finally
{
    Directory.Delete(dir, true);
}
