using System;
using System.Threading;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleHookInt32Zero : BunnyHookModule
{
    public static readonly bool EnableAtLoad = true;
    private const int SampleLimit = 2;
    private static BunnyMethodHook? activeHook;
    private static int installed;
    public static long Calls;
    public static string LastError { get; private set; } = "";

    public static string Result()
    {
        long calls = Interlocked.Read(ref Calls);
        if (calls > 0) return "OK" + calls;
        if (Volatile.Read(ref installed) != 0) return "ARMADO";
        int index = LastError.IndexOf("codigo ", StringComparison.Ordinal);
        if (index >= 0) return "E" + LastError.Substring(index + 7).Replace(")", "");
        return LastError.Length == 0 ? "AGUARDANDO" : "ERRO";
    }

    public override string DiagnosticStatus() => Result();

    public override void Load(BunnyHookScope hooks)
    {
        if (!EnableAtLoad) return;
        try
        {
            activeHook = hooks.HookInt32Zero("Terraria.Utilities.UnifiedRandom", "Next", (original, self) =>
            {
                int vanilla = original.Invoke();
                if (Interlocked.Increment(ref Calls) >= SampleLimit)
                    activeHook?.Dispose();
                return vanilla;
            }, isStatic: false);
            Volatile.Write(ref installed, 1);
            LastError = "";
        }
        catch (Exception error)
        {
            Volatile.Write(ref installed, 0);
            LastError = error.GetType().Name + ": " + error.Message;
        }
    }

    public override void Unload()
    {
        activeHook?.Dispose();
        activeHook = null;
        Volatile.Write(ref installed, 0);
    }
}
