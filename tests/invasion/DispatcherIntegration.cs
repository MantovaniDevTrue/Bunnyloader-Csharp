using BunnyLoader.Source;
using ExampleModCSharp;

static class InvasionDispatcherTest
{
    private static int tests;

    private static void Check(bool result, string name)
    {
        if (!result) throw new InvalidOperationException(name);
        tests++;
    }

    public static int Main()
    {
        try
        {
            ExampleInvasion.WorldExit();
            const int npcType = 700;
            int modSlot = BunnyEntitySourceDispatcher.Register(
                "example-invasion-test", typeof(ExampleSlimeNPC).Assembly.Location);
            Check(modSlot >= 0, "assembly registered");
            Check(BunnyEntitySourceDispatcher.Bind(modSlot, 0, npcType,
                nameof(ExampleSlimeNPC)), "NPC bound");
            var beacon = new ExampleInvasionBeacon();
            Check(!beacon.OnUse(0, new BunnyUseContext(62)) &&
                beacon.OnUseRejectionReason(0, new BunnyUseContext(62)) == "Nighttime",
                "nighttime prevents start with explicit reason");
            Check(!beacon.OnUse(0, new BunnyUseContext(61)) &&
                beacon.OnUseRejectionReason(0, new BunnyUseContext(61)) == "NotAtSurface",
                "underground prevents start");
            Check(!beacon.OnUse(0, new BunnyUseContext(59)) &&
                beacon.OnUseRejectionReason(0, new BunnyUseContext(59)) == "VanillaInvasionActive",
                "vanilla invasion prevents start");
            Check(beacon.OnUse(0, new BunnyUseContext(63)),
                "valid daytime surface use starts invasion");
            Check(!beacon.OnUse(0, new BunnyUseContext(63)) &&
                beacon.OnUseRejectionReason(0, new BunnyUseContext(63)) == "AlreadyRunning",
                "duplicate use refuses without resetting progress");
            ExampleInvasion.WorldExit();
            Check(ExampleInvasion.Start(true), "invasion started");

            Check(ExampleInvasion.TrySpawn(true, 0, out var requested),
                "spawn reserved");
            Check(ExampleInvasion.NativeSpawnCreated(true, requested.Epoch,
                requested.Ticket, 11, 98765L), "native ticket associated with slot");
            long velocity = BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 11, 42, 25, 25, 100f, 100f,
                0f, 0f, 200f, 100f, 98765L);
            Check(velocity >= -1 && ExampleInvasion.Controller.Progress == 0 &&
                ExampleInvasion.Controller.ActiveCount == 1 &&
                ExampleInvasion.Pipeline.PendingCount == 0, "native OnSpawn confirms identity and generation");

            int before = ExampleSlimeNPC.KillEvents;
            Check(BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 11, 42),
                "dispatcher OnKill executed");
            Check(ExampleSlimeNPC.KillEvents == before + 1 &&
                ExampleInvasion.Controller.Progress == 1, "invasion credits eligible NPC");
            Check(!BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 11, 42) &&
                ExampleInvasion.Controller.Progress == 1, "double death rejected");

            BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 12, 43, 25, 25, 100f, 100f, 0f, 0f, 200f, 100f);
            Check(BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 12, 43) &&
                ExampleInvasion.Controller.Progress == 1,
                "unadmitted NPC excluded");

            for (int tick = 0; tick < 120; tick++)
                ExampleInvasion.Pipeline.Tick(true, true);
            Check(ExampleInvasion.TrySpawn(true, 0, out var next),
                "second wave reserved");
            Check(ExampleInvasion.NativeSpawnFailed(true, next.Epoch, next.Ticket),
                "failed spawn reclaimed");

            BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 13, 44, 25, 25, 100f, 100f, 0f, 0f, 200f, 100f);
            BunnyEntitySourceDispatcher.ResetWorld();
            Check(ExampleInvasion.Controller.Progress == 1, "despawn not a kill");
            ExampleInvasion.WorldExit();
            Check(ExampleInvasion.Controller.Progress == 0 &&
                ExampleInvasion.Controller.Phase == BunnyInvasionPhase.Inactive,
                "world cleanup");
            Console.WriteLine("ExampleInvasion dispatcher: " + tests + " checks OK");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
