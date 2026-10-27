using System;
using System.Threading;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleHookVoidInt32Triple : BunnyHookModule
{
    public static readonly bool EnableAtLoad = true;
    private const int SampleLimit = 2;
    private static readonly string[] Types = { "Terraria.WorldGen", "Terraria.WorldGen", "Terraria.WorldGen", "Terraria.WorldItem" };
    private static readonly string[] Methods = { "Check1x1", "CheckLogicTiles", "Check2x2", "SpawnShimmeredItem" };
    private static readonly bool[] StaticFlags = { true, true, true, false };
    private static BunnyMethodHook? activeHook;
    private static int installed;
    public static long Calls;
    public static string Selected { get; private set; } = "";
    public static string LastError { get; private set; } = "";
    public static string LastAttempt { get; private set; } = "";

    public static string Result()
    {
        long calls = Interlocked.Read(ref Calls);
        if (calls > 0) return "OK" + calls + "@" + Selected;
        if (Volatile.Read(ref installed) != 0) return "ARMADO@" + Selected;
        int index = LastError.IndexOf("codigo ", StringComparison.Ordinal);
        if (index >= 0) return "E" + LastError.Substring(index + 7).Replace(")", "") + "@" + LastAttempt;
        return LastError.Length == 0 ? "AGUARDANDO" : "ERRO@" + LastAttempt;
    }

    public override string DiagnosticStatus() => Result();

    public override void Load(BunnyHookScope hooks)
    {
        if (!EnableAtLoad) return;
        for (int i = 0; i < Methods.Length; ++i)
        {
            LastAttempt = Types[i] + "." + Methods[i];
            try
            {
                activeHook = hooks.HookVoidInt32Triple(Types[i], Methods[i], (original, self, first, second, third) =>
                {
                    original.Invoke();
                    if (Interlocked.Increment(ref Calls) >= SampleLimit)
                        activeHook?.Dispose();
                }, isStatic: StaticFlags[i]);
                Selected = LastAttempt;
                LastError = "";
                Volatile.Write(ref installed, 1);
                return;
            }
            catch (Exception error)
            {
                LastError = error.GetType().Name + ": " + error.Message;
                if (LastError.Contains("codigo -11") || LastError.Contains("codigo -12") ||
                    LastError.Contains("codigo -10") || LastError.Contains("codigo -3"))
                    return;
            }
        }
        Volatile.Write(ref installed, 0);
    }

    public override void Unload()
    {
        activeHook?.Dispose();
        activeHook = null;
        Volatile.Write(ref installed, 0);
    }
}
