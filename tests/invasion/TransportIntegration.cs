using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp;

internal static class TransportIntegration
{
    [DllImport("bunny", EntryPoint = "bunny_invasion_test_world")]
    private static extern void SetWorld(int ready, long session);

    [DllImport("bunny", EntryPoint = "bunny_invasion_test_pump")]
    private static extern void PumpNative();

    [DllImport("bunny", EntryPoint = "bunny_item_use_test_inject")]
    private static extern void InjectItemUse(int playerIndex);

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
        ExampleInvasion.WorldExit();
        SetWorld(1, 11);
        Check(BunnyInvasionNativeTransport.WorldSession == 11, "managed PInvoke session");
        int mod = BunnyEntitySourceDispatcher.Register(
            "mod-a", typeof(ExampleSlimeNPC).Assembly.Location);
        Check(mod >= 0, "reflection registers mod and invasion system");
        const int npcType = 700;
        Check(BunnyEntitySourceDispatcher.Bind(mod, 0, npcType,
            nameof(ExampleSlimeNPC)), "bind NPC");
        SetItemReady(0);
        Check(Tick(mod, 11) == 0, "world activation queues item binding and resets stale state");
        Check(ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Inactive,
            "no automatic invasion before item use");
        InjectItemUse(0);
        Check(Tick(mod, 11) == 0 &&
            ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Inactive,
            "native item hook not acknowledged: no managed invocation");
        SetItemReady(1);
        Check(Tick(mod, 11) == 1, "native-ready item OnUse starts invasion and queues NPC spawn");
        int epoch = ExampleInvasion.Controller.Snapshot().Epoch;
        InjectItemUse(0);
        Check(Tick(mod, 11) == 0 &&
            ExampleInvasion.Controller.Snapshot().Epoch == epoch,
            "repeated item use cannot restart running invasion");
        Check(ExampleInvasion.Pipeline.PendingCount == 1, "reservation created");
        PumpNative();
        Check(Tick(mod, 11) == 1, "MonoVM receives native spawn reply");
        Check(ExampleInvasion.Pipeline.PendingCount == 1 &&
            ExampleInvasion.Controller.ActiveCount == 0, "wait for native NPC AI identity");
        long motion = BunnyEntitySourceDispatcher.TickNpc(mod, npcType, 1,
            50, 25, 25, 1710, 2150, 0, 0, 1700, 2200, 1001L);
        Check(motion >= -1 && ExampleInvasion.Controller.ActiveCount == 1 &&
            ExampleInvasion.Pipeline.PendingCount == 0, "real NPC OnSpawn confirms identity");
        Check(BunnyEntitySourceDispatcher.KillNpc(mod, npcType, 1, 50) &&
            ExampleInvasion.Controller.Progress == 1, "real kill scores once");
        Check(!BunnyEntitySourceDispatcher.KillNpc(mod, npcType, 1, 50),
            "double kill ignored");

        SetWorld(0, 12);
        SetWorld(1, 12);
        Check(Tick(mod, 12) == 0 && ExampleInvasion.Controller.Progress == 0 &&
            ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Inactive,
            "new Terraria world resets old invasion");
        InjectItemUse(0);
        Check(Tick(mod, 12) == 1 &&
            ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Running,
            "new world item use starts a fresh invasion");
        SetWorld(0, 13);
        SetWorld(1, 13);
        Check(Tick(mod, 13) == 0 && ExampleInvasion.Pipeline.PendingCount == 0,
            "old world queued spawn discarded");
        Console.WriteLine("Managed-to-native-to-managed invasion bridge: " + checks + " checks OK");
        return 0;
    }
}
