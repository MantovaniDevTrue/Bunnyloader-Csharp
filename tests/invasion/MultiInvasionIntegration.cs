using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp;

public sealed class OtherInvasionSystem : BunnyInvasionSystem
{
    public static readonly BunnyInvasionController Controller = new(
        new BunnyInvasionDefinition("other-invasion", 3, 2, 1,
            new BunnyInvasionEnemy(nameof(ExampleSlimeNPC), 1, 1)));
    public static readonly BunnyInvasionSpawnPipeline Spawn = new(Controller);
    public override BunnyInvasionSpawnPipeline Pipeline => Spawn;
}

internal static class MultiInvasionIntegration
{
    [DllImport("bunny", EntryPoint = "bunny_invasion_test_world")]
    private static extern void World(int ready, long session);
    [DllImport("bunny", EntryPoint = "bunny_invasion_test_pump")]
    private static extern void Pump();
    private static int checks;

    private static void Check(bool test, string label)
    {
        if (!test) throw new InvalidOperationException(label);
        checks++;
    }

    public static int Main()
    {
        // Isolate repeated host runs from an earlier aborted test sidecar.
        foreach (int id in new[] { 81 })
            System.IO.File.Delete("/tmp/bunny-invasion-ci-" + id + ".wld.invasions.bl");
        World(1, 81);
        const string modId = "mod-a";
        string assemblyPath = typeof(OtherInvasionSystem).Assembly.Location;
        using var shared = BunnyModAssemblyRegistry.Acquire(modId, assemblyPath);
        var exampleController = (BunnyInvasionController)shared.Assembly
            .GetType("ExampleModCSharp.ExampleInvasion", true)!.GetField("Controller")!.GetValue(null)!;
        var otherController = (BunnyInvasionController)shared.Assembly
            .GetType(nameof(OtherInvasionSystem), true)!.GetField("Controller")!.GetValue(null)!;
        int mod = BunnyEntitySourceDispatcher.Register(modId, assemblyPath);
        Check(mod >= 0, "register multiple invasion systems");
        Check(BunnyEntitySourceDispatcher.Bind(mod, 0, 700, nameof(ExampleSlimeNPC)),
            "bind NPC");
        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 81, 0, 1700, 2200);
        Check(exampleController.Start(true), "first invasion starts");
        Check(otherController.Start(true), "second invasion starts");

        Check(BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 81, 0, 1700, 2200) >= 1,
            "first native spawn submitted");
        Pump();
        Check(BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 81, 0, 1700, 2200) >= 1,
            "first reply and second spawn submitted");
        Pump();
        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 81, 0, 1700, 2200);
        for (int slot = 1; slot <= 2; slot++)
            BunnyEntitySourceDispatcher.TickNpc(mod, 700, slot, 40 + slot,
                25, 25, 1500f, 1800f, 0f, 0f, 1700f, 2200f, 1000L + slot);

        Check(exampleController.ActiveCount +
            otherController.ActiveCount == 2,
            "both invasions admit only their own native NPC");
        Check(exampleController.ActiveCount == 1 &&
            otherController.ActiveCount == 1,
            "separate scorekeeping");
        Check(BunnyEntitySourceDispatcher.KillNpc(mod, 700, 1, 41),
            "first kill delivered");
        Check(exampleController.Progress +
            otherController.Progress == 1,
            "one kill scores exactly one invasion");
        Check(BunnyEntitySourceDispatcher.KillNpc(mod, 700, 2, 42),
            "second kill delivered");
        Check(exampleController.Progress == 1 &&
            otherController.Progress == 1,
            "each invasion scores once");
        World(0, 82);
        World(1, 82);
        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 82, 0, 1700, 2200);
        Check(exampleController.Phase == BunnyInvasionPhase.Inactive &&
            otherController.Phase == BunnyInvasionPhase.Inactive,
            "independent world isolation");
        World(0, 83);
        System.IO.File.Delete("/tmp/bunny-invasion-ci-81.wld.invasions.bl");
        Console.WriteLine("Multi-invasion native roundtrip: " + checks + " checks OK");
        return 0;
    }
}
