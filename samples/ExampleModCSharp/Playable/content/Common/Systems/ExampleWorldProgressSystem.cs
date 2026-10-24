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

    public override void ClearWorld()
    {
        DownedExampleBoss = false;
        WorldFrames = 0;
    }

    public override void SaveWorldData(BunnyWorldData tag)
    {
        if (DownedExampleBoss)
            tag.SetBool("downedExampleBoss", true);
    }

    public override void LoadWorldData(BunnyWorldData tag)
    {
        DownedExampleBoss = tag.ContainsKey("downedExampleBoss");
    }

    public override void NetSend(BunnyNetWriter writer)
    {
        writer.WriteFlags(DownedExampleBoss);
    }

    public override void NetReceive(BunnyNetReader reader)
    {
        bool[] flags = reader.ReadFlags();
        if (flags.Length != 1)
            throw new System.FormatException("Invalid boss progression packet");
        DownedExampleBoss = flags[0];
    }

    public override void OnWorldTick(in BunnyWorldContext world)
    {
        if (world.IsAuthoritative)
            WorldFrames += world.ElapsedFrames;
    }

    public override void OnWorldExit()
    {
        ClearWorld();
    }
}
