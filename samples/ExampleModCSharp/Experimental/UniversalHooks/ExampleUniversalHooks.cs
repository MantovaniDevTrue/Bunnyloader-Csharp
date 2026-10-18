using BunnyLoader.Source;

public sealed class ExampleUniversalHooks : BunnyHookModule
{
    public static long ResetCalls { get; private set; }
    public override void Load(BunnyHookScope hooks)
    {
        BunnyHooks.Hook(hooks, "Terraria.Player", "ResetEffects", (original, self) =>
        {
            original.Invoke();
            ResetCalls++;
        });
    }
}
