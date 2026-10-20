using System.Threading;
using BunnyLoader.Source;

public sealed class UniversalHookLifecycleFixture : BunnyHookModule
{
    public static int Loads;
    public static int Unloads;

    public override void Load(BunnyHookScope scope)
    {
        Interlocked.Increment(ref Loads);
    }

    public override void Unload()
    {
        Interlocked.Increment(ref Unloads);
    }

    public override string DiagnosticStatus() => "active";
}
