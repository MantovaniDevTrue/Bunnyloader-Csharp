using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp;

internal static class TransportIntegration
{
    [DllImport("bunny", EntryPoint = "bunny_invasion_test_world")]
    private static extern void SetWorld(int ready, long session);

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_pump")]
    private static extern void PumpNative();

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_reward_pump")]
    private static extern void PumpReward();

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_reward_type")]
    private static extern int RewardItemType();

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_reward_stack")]
    private static extern int RewardStack();

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_reward_player")]
    private static extern int RewardPlayer();

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_world_restore")]
    private static extern void SetWorldRestore(int ready, long session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("bunny", EntryPoint = "bunny_item_use_test_inject")]
    private static extern void InjectItemUse(int playerIndex);

    [DllImport("bunny", EntryPoint = "bunny_item_use_test_inject_context")]
    private static extern void InjectItemUseContext(int playerIndex, int flags);

    [DllImport("bunny", EntryPoint = "bunny_item_use_test_ready")]
    private static extern void SetItemReady(int ready);

    private static int checks;

    private static void Check(bool value, string description)
    {
        if (!value) throw new InvalidOperationException(description);
        checks++;
    }

    private static int Tick(int modSlot, long world) =>
        BunnyEntitySourceDispatcher.TickWorld(modSlot, 0, 30, world,
            0, 1700f, 2200f);

    public static int Main()
    {
        // Isolate repeated host runs from an earlier aborted test sidecar.
        foreach (int id in new[] { 11,12,13 })
            System.IO.File.Delete("/tmp/bunny-invasion-ci-" + id + ".wld.invasions.bl");
        const string modId = "mod-a";
        string sourcePath = typeof(ExampleSlimeNPC).Assembly.Location;
        using var modAssembly = BunnyModAssemblyRegistry.Acquire(modId, sourcePath);
        Type invasionType = modAssembly.Assembly.GetType("ExampleModCSharp.ExampleInvasion", true)!;
        var controller = (BunnyInvasionController)invasionType.GetField("Controller")!.GetValue(null)!;
        var pipeline = (BunnyInvasionSpawnPipeline)invasionType.GetField("Pipeline")!.GetValue(null)!;
        pipeline.ResetWorld();
        SetWorld(1, 11);
        Check(BunnyInvasionNativeTransport.WorldSession == 11, "managed PInvoke session");
        int mod = BunnyEntitySourceDispatcher.Register(modId, sourcePath);
        Check(mod >= 0, "reflection registers mod and invasion system");
        const int npcType = 700;
        Check(BunnyEntitySourceDispatcher.Bind(mod, 0, npcType,
            nameof(ExampleSlimeNPC)), "bind NPC");
        SetItemReady(0);
        Check(BunnyEntitySourceDispatcher.TickWorld(mod, 1, 30, 11,
            0, 1700f, 2200f) == 0 &&
            controller.Phase == BunnyInvasionPhase.Inactive,
            "client world tick only registers item hooks, not the invasion");
        Check(Tick(mod, 11) == 0, "world activation queues item binding and resets stale state");
        Check(controller.Phase == BunnyInvasionPhase.Inactive,
            "no automatic invasion before item use");
        InjectItemUse(0);
        Check(Tick(mod, 11) == 0 &&
            controller.Phase == BunnyInvasionPhase.Inactive,
            "native item hook not acknowledged: no managed invocation");
        SetItemReady(1);
        Check(Tick(mod, 11) == 1, "native-ready item OnUse starts invasion and queues NPC spawn");
        int epoch = controller.Snapshot().Epoch;
        InjectItemUse(0);
        Check(Tick(mod, 11) == 1 &&
            controller.Snapshot().Epoch == epoch,
            "repeated item use cannot restart running invasion");
        Check(pipeline.PendingCount == 1, "reservation created");
        PumpNative();
        Check(Tick(mod, 11) == 1, "MonoVM receives native spawn reply");
        Check(pipeline.PendingCount == 1 &&
            controller.ActiveCount == 0, "wait for native NPC AI identity");
        long motion = BunnyEntitySourceDispatcher.TickNpc(mod, npcType, 1,
            50, 25, 25, 1710, 2150, 0, 0, 1700, 2200, 1001L);
        Check(motion >= -1 && controller.ActiveCount == 1 &&
            pipeline.PendingCount == 0, "real NPC OnSpawn confirms identity");
        Check(BunnyEntitySourceDispatcher.KillNpc(mod, npcType, 1, 50) &&
            controller.Progress == 1, "real kill scores once");
        Check(System.IO.File.Exists("/tmp/bunny-invasion-ci-11.wld.invasions.bl"),
            "real native sidecar written immediately on NPC kill");
        Check(!BunnyEntitySourceDispatcher.KillNpc(mod, npcType, 1, 50),
            "double kill ignored");
        var beforeFinish = controller.Snapshot();
        Check(controller.Restore(true,
            beforeFinish with {
                Phase = BunnyInvasionPhase.Completed,
                Progress = controller.Definition.Goal,
                Active = 0, Pending = 0, Cooldown = 0
            }), "simulate completed goal for native reward bridge");
        Tick(mod, 11);
        PumpReward();
        Check(RewardItemType() == 6215 && RewardStack() == 5 &&
            RewardPlayer() == 0, "native Bunny giveItem reward delivered once");
        Tick(mod, 11);
        PumpReward();
        Check(RewardStack() == 5, "completed reward not duplicated");

        SetWorld(0, 12);
        SetWorld(1, 12);
        Check(Tick(mod, 12) == 0 && controller.Progress == 0 &&
            controller.Phase == BunnyInvasionPhase.Inactive,
            "new Terraria world resets old invasion");
        foreach (int requiredFlag in new[] { 1, 2, 4, 8, 16 })
        {
            InjectItemUseContext(0, 31 & ~requiredFlag);
            Check(Tick(mod, 12) == 1 &&
                controller.Phase == BunnyInvasionPhase.Inactive &&
                pipeline.PendingCount == 0,
                "native item context prevents starting invasion when a required flag is missing");
        }
        InjectItemUse(0);
        Check(Tick(mod, 12) == 1 &&
            controller.Phase == BunnyInvasionPhase.Running,
            "new world item use starts a fresh invasion");
        SetWorld(0, 13);
        SetWorld(1, 13);
        Check(Tick(mod, 13) == 0 && pipeline.PendingCount == 0,
            "old world queued spawn discarded");
        SetWorldRestore(1, 20, "/tmp/bunny-invasion-ci-11.wld");
        Tick(mod, 20);
        Check(controller.Phase == BunnyInvasionPhase.Completed &&
            controller.Progress == controller.Definition.Goal &&
            controller.ActiveCount == 0,
            "re-entered original world restores completion and never reuses NPC membership");
        SetWorld(0, 21);
        foreach (int id in new[] { 11, 12, 13 })
            System.IO.File.Delete("/tmp/bunny-invasion-ci-" + id + ".wld.invasions.bl");
        Console.WriteLine("Managed-to-native-to-managed invasion bridge: " + checks + " checks OK");
        return 0;
    }
}
