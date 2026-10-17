using System;
using System.Threading;
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleHookVoidInt32 : BunnyHookModule
{
    public const bool EnableAtLoad = false;
    public static long Calls;
    public static string LastError { get; private set; } = "";

    public override void Load(BunnyHookScope hooks)
    {
        if (!EnableAtLoad) return;
        try
        {
            hooks.HookVoidInt32("Terraria.WorldGen", "QuickFindHome", (original, self, npc) =>
            {
                original.Invoke();
                System.Threading.Interlocked.Increment(ref Calls);
            }, isStatic: true);
            LastError = "";
        }
        catch (Exception error)
        {
            LastError = error.GetType().Name + ": " + error.Message;
        }
    }
}
