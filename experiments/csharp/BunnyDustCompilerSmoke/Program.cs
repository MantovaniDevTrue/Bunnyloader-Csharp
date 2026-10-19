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
""");
    string output = Path.Combine(dir, "ExampleDust.dll");
    string diag = Path.Combine(dir, "compile.txt");
    int result = Compiler.Compile(root, output, referenceDirectory, diag);
    if (result != 1)
        throw new Exception("Compiler returned " + result + "\n" + File.ReadAllText(diag));
    int classes = CompiledAssemblyProbe.Verify(output, diag);
    if (classes != 1)
        throw new Exception("Probe returned " + classes + "\n" + File.ReadAllText(diag));
    string wire = File.ReadAllText(Path.Combine(dir, "source-runtime-values.txt"));
    if (!wire.Contains("DUST\tExampleDust\t6\t5\tContent/Dusts/Example.png\n",
        StringComparison.Ordinal) ||
        !wire.StartsWith("BUNNY_MANAGED_DEFAULTS_V2\n", StringComparison.Ordinal) ||
        wire.Split('\n', StringSplitOptions.RemoveEmptyEntries).Length != 2)
        throw new Exception("Dust defaults wire mismatch:\n" + wire);
    Console.WriteLine("DUST_SOURCE_ROSLYN_SMOKE=PASS");
}
finally
{
    Directory.Delete(dir, true);
}
