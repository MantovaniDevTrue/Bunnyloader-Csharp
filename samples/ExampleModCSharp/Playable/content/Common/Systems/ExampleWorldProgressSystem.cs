using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWorldProgressSystem : BunnyWorldSystem
{
    public static bool DownedExampleBoss { get; private set; }
    public static long WorldFrames { get; private set; }

    public static void MarkBossDefeated()
    {
        DownedExampleBoss = true;
    }

    public override void OnWorldEnter(in BunnyWorldContext world)
    {
        DownedExampleBoss = false;
        WorldFrames = 0;
    }

    public override void OnWorldTick(in BunnyWorldContext world)
    {
        if (world.IsAuthoritative)
            WorldFrames += world.ElapsedFrames;
    }

    public override void OnWorldExit()
    {
        DownedExampleBoss = false;
        WorldFrames = 0;
    }
}
