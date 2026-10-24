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
            const string modId = "example-invasion-test";
            string assemblyPath = typeof(ExampleSlimeNPC).Assembly.Location;
            using var mod = BunnyModAssemblyRegistry.Acquire(modId, assemblyPath);
            var invasion = mod.Assembly.GetType("ExampleModCSharp.ExampleInvasion", true)!;
            var controller = (BunnyInvasionController)invasion.GetField("Controller")!.GetValue(null)!;
            var pipeline = (BunnyInvasionSpawnPipeline)invasion.GetField("Pipeline")!.GetValue(null)!;
            var slime = mod.Assembly.GetType("ExampleModCSharp.ExampleSlimeNPC", true)!;
            int KillEvents() => (int)slime.GetField("KillEvents")!.GetValue(null)!;
            pipeline.ResetWorld();
            const int npcType = 700;
            int modSlot = BunnyEntitySourceDispatcher.Register(modId, assemblyPath);
            Check(modSlot >= 0, "assembly registered");
            Check(BunnyEntitySourceDispatcher.Bind(modSlot, 0, npcType,
                nameof(ExampleSlimeNPC)), "NPC bound");
            var beacon = (BunnyItem)Activator.CreateInstance(
                mod.Assembly.GetType("ExampleModCSharp.ExampleInvasionBeacon", true)!)!;
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
            pipeline.ResetWorld();
            Check(controller.Start(true), "invasion started");

            Check(pipeline.TryReserve(true, 0, out var requested),
                "spawn reserved");
            Check(pipeline.TryGetReservedClass(requested.Epoch, requested.Ticket, out string className) &&
                pipeline.ReportCreated(true, requested.Epoch,
                    requested.Ticket, className, 11, 98765L), "native ticket associated with slot");
            long velocity = BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 11, 42, 25, 25, 100f, 100f,
                0f, 0f, 200f, 100f, 98765L);
            Check(velocity >= -1 && controller.Progress == 0 &&
                controller.ActiveCount == 1 &&
                pipeline.PendingCount == 0, "native OnSpawn confirms identity and generation");

            int before = KillEvents();
            Check(BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 11, 42),
                "dispatcher OnKill executed");
            Check(KillEvents() == before + 1 &&
                controller.Progress == 1, "invasion credits eligible NPC");
            Check(!BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 11, 42) &&
                controller.Progress == 1, "double death rejected");

            BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 12, 43, 25, 25, 100f, 100f, 0f, 0f, 200f, 100f);
            Check(BunnyEntitySourceDispatcher.KillNpc(modSlot, npcType, 12, 43) &&
                controller.Progress == 1,
                "unadmitted NPC excluded");

            for (int tick = 0; tick < 120; tick++)
                pipeline.Tick(true, true);
            Check(pipeline.TryReserve(true, 0, out var next),
                "second wave reserved");
            Check(pipeline.ReportFailed(true, next.Epoch, next.Ticket),
                "failed spawn reclaimed");

            BunnyEntitySourceDispatcher.TickNpc(
                modSlot, npcType, 13, 44, 25, 25, 100f, 100f, 0f, 0f, 200f, 100f);
            BunnyEntitySourceDispatcher.ResetWorld();
            Check(controller.Progress == 1, "despawn not a kill");
            pipeline.ResetWorld();
            Check(controller.Progress == 0 &&
                controller.Phase == BunnyInvasionPhase.Inactive,
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
