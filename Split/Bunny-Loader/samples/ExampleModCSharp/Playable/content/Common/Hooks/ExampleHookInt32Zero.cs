using System;
using System.Threading;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleHookInt32Zero : BunnyHookModule
{
    public const bool EnableAtLoad = false;
    public static long Calls;
    public static bool OverrideReturn;
    public static string LastError { get; private set; } = "";

    public override void Load(BunnyHookScope hooks)
    {
        if (!EnableAtLoad) return;
        try
        {
            hooks.HookInt32Zero("Terraria.Utilities.UnifiedRandom", "Next", (original, self) =>
            {
                int value = original.Invoke();
                System.Threading.Interlocked.Increment(ref Calls);
                return OverrideReturn ? 0 : value;
            }, isStatic: false);
            LastError = "";
        }
        catch (Exception error)
        {
            LastError = error.GetType().Name + ": " + error.Message;
        }
    }
}
