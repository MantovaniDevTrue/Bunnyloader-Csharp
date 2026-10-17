using System;
using System.Threading;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleIntPairHook : BunnyHookModule
{
    private static readonly object gate = new();
    private static BunnyHookScope? scope;
    private static BunnyMethodHook? installed;

    public static long Calls;
    public static long Modified;
    public static bool OverrideSmallRange;
    public const bool EnableHookAtLoad = false;
    public static string LastError { get; private set; } = "";

    public override void Load(BunnyHookScope hooks)
    {
        lock (gate)
            scope = hooks;
        if (EnableHookAtLoad)
            Start();
    }

    public static string Start()
    {
        lock (gate)
        {
            if (installed is not null && !installed.IsDisposed)
                return "Hook int(int,int) ja instalado";
            if (scope is null)
                return "Modulo C# ainda nao carregado";
            try
            {
                installed = scope.HookInt32Pair("Terraria.Utilities.UnifiedRandom", "Next",
                    (original, self, min, max) =>
                    {
                        int vanilla = original.Invoke();
                        Interlocked.Increment(ref Calls);
                        if (OverrideSmallRange && min >= 0 && max > min + 1 && max <= 16 && vanilla != min)
                        {
                            Interlocked.Increment(ref Modified);
                            return min;
                        }
                        return vanilla;
                    });
                LastError = "";
                return "Hook int(int,int) instalado; interrompa apos o teste";
            }
            catch (Exception error)
            {
                LastError = error.GetType().Name + ": " + error.Message;
                return LastError;
            }
        }
    }

    public static string Stop()
    {
        lock (gate)
        {
            OverrideSmallRange = false;
            installed?.Dispose();
            installed = null;
            return "Hook int(int,int) desativado";
        }
    }

    public static string Status() => "int(int,int): chamadas=" + Interlocked.Read(ref Calls) +
        " modificados=" + Interlocked.Read(ref Modified) + " ativo=" + (installed is not null && !installed.IsDisposed) +
        " erro=" + LastError;

    public override void Unload()
    {
        Stop();
        lock (gate)
            scope = null;
    }
}
