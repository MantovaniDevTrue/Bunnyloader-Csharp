using System;
using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using BunnyLoader.Source;

public sealed class UniversalHookLifecycleFixture : BunnyHookModule
{
    public override void Load(BunnyHookScope scope) { }
    public override string DiagnosticStatus() => "active";
}

public static class UniversalHookUnloadProbe
{
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference CaptureContext(string id)
    {
        var owner = typeof(BunnyUniversalHookDispatcher);
        FieldInfo table = owner.GetField("Contexts", BindingFlags.Static | BindingFlags.NonPublic)!;
        var entries = (IEnumerable)table.GetValue(null)!;
        foreach (object entry in entries)
        {
            Type type = entry.GetType();
            if ((string)type.GetField("Id")!.GetValue(entry)! != id) continue;
            var lease = type.GetField("Lease")!.GetValue(entry)!;
            var assembly = (Assembly)lease.GetType().GetProperty("Assembly",
                BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(lease)!;
            var loader = AssemblyLoadContext.GetLoadContext(assembly)
                ?? throw new InvalidOperationException("Missing universal load context");
            if (!loader.IsCollectible)
                throw new InvalidOperationException("Universal context is not collectible");
            if (!ReferenceEquals(AssemblyLoadContext.GetLoadContext(assembly), loader))
                throw new InvalidOperationException("Universal assembly escaped context");
            if (!typeof(BunnyHookModule).IsAssignableFrom(
                    assembly.GetType(nameof(UniversalHookLifecycleFixture), true)!))
                throw new InvalidOperationException("Universal module SDK type mismatch");
            return new WeakReference(loader);
        }
        throw new InvalidOperationException("Missing universal mod " + id);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static (WeakReference Alpha, WeakReference Beta) SetupAndUnloadAlpha(string path)
    {
        BunnyUniversalHookDispatcher.UnloadAll();
        if (BunnyUniversalHookDispatcher.Register("hook-alpha", path) != 1 ||
            BunnyUniversalHookDispatcher.Register("hook-beta", path) != 1)
            throw new InvalidOperationException("Universal contexts failed registration");
        if (BunnyUniversalHookDispatcher.GetStatusSnapshot("hook-alpha") !=
                "UniversalHookLifecycleFixture=active" ||
            BunnyUniversalHookDispatcher.GetStatusSnapshot("hook-beta") !=
                "UniversalHookLifecycleFixture=active")
            throw new InvalidOperationException("Universal contexts were not independent");
        WeakReference alpha = CaptureContext("hook-alpha");
        WeakReference beta = CaptureContext("hook-beta");
        if (!BunnyUniversalHookDispatcher.Unload("hook-alpha") ||
            BunnyUniversalHookDispatcher.GetStatusSnapshot("hook-alpha") != "nao registrado" ||
            BunnyUniversalHookDispatcher.GetStatusSnapshot("hook-beta") !=
                "UniversalHookLifecycleFixture=active" ||
            BunnyUniversalHookDispatcher.Unload("hook-alpha") ||
            BunnyUniversalHookDispatcher.Unload(""))
            throw new InvalidOperationException("Scoped unload affected independent mod");
        return (alpha, beta);
    }

    private static void AssertCollected(WeakReference target)
    {
        for (int i = 0; i < 20 && target.IsAlive; ++i)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            System.Threading.Thread.Sleep(10);
        }
        if (target.IsAlive)
            throw new InvalidOperationException("Collectible universal context survived unload");
    }

    public static void Run(string path)
    {
        var (alpha, beta) = SetupAndUnloadAlpha(path);
        AssertCollected(alpha);
        if (!beta.IsAlive)
            throw new InvalidOperationException("Unrelated universal context unloaded unexpectedly");
        BunnyUniversalHookDispatcher.UnloadAll();
        if (BunnyUniversalHookDispatcher.GetStatusSnapshot("hook-beta") != "nao registrado")
            throw new InvalidOperationException("Universal contexts remained registered");
        AssertCollected(beta);
        Console.WriteLine("UNIVERSAL_MANAGED_COLLECTIBLE_UNLOAD_OK");
    }
}
