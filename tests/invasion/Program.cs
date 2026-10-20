using BunnyLoader.Source;

static class InvasionTest
{
    private static int passed;

    private static void Check(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
        passed++;
    }

    private static void Invalid(Action action, string reason)
    {
        try { action(); }
        catch (ArgumentException) { passed++; return; }
        throw new InvalidOperationException(reason);
    }

    private static void Run()
    {
        Invalid(() => new BunnyInvasionDefinition("", 10, 2, 5,
            new BunnyInvasionEnemy("Scout", 1, 1)), "empty id");
        Invalid(() => new BunnyInvasionDefinition("test", 0, 2, 5,
            new BunnyInvasionEnemy("Scout", 1, 1)), "zero goal");
        Invalid(() => new BunnyInvasionDefinition("test", 10, 0, 5,
            new BunnyInvasionEnemy("Scout", 1, 1)), "zero concurrency");
        Invalid(() => new BunnyInvasionDefinition("test", 10, 2, 0,
            new BunnyInvasionEnemy("Scout", 1, 1)), "zero interval");
        Invalid(() => new BunnyInvasionDefinition("test", 10, 2, 5,
            new BunnyInvasionEnemy("Scout", 1, 1),
            new BunnyInvasionEnemy("Scout", 1, 2)), "duplicated NPC");

        var definition = new BunnyInvasionDefinition("example", 5, 2, 3,
            new BunnyInvasionEnemy("Scout", 1, 1),
            new BunnyInvasionEnemy("Brute", 2, 3));
        var invasion = new BunnyInvasionController(definition);

        Check(!invasion.Start(false) && !invasion.CanSpawn, "authority start");
        Check(invasion.Start(true) && invasion.CanSpawn, "start");
        Check(!invasion.Start(true), "double start");
        Check(!invasion.TryReserveSpawn(false, 0, out _), "authority spawn");
        Check(invasion.TryReserveSpawn(true, 0, out var a) &&
            a.NpcClass == "Scout" && invasion.PendingCount == 1, "weighted scout");
        Check(!invasion.TryReserveSpawn(true, 1, out _), "cooldown");
        Check(!invasion.CommitSpawn(false, a.Ticket, "Scout", 3, 10), "authority commit");
        Check(!invasion.CommitSpawn(true, a.Ticket, "Brute", 3, 10), "wrong class");
        Check(!invasion.CommitSpawn(true, a.Ticket, "Scout", -1, 10), "bad slot");
        Check(invasion.CommitSpawn(true, a.Ticket, "Scout", 3, 10), "commit");
        Check(!invasion.CommitSpawn(true, a.Ticket, "Scout", 3, 10), "duplicate commit");
        Check(invasion.ActiveCount == 1 && invasion.PendingCount == 0, "accounting");

        invasion.Tick(false, true);
        invasion.Tick(true, false);
        Check(!invasion.CanSpawn, "cooldown pauses without authority or player");
        for (int i = 0; i < 3; ++i) invasion.Tick(true, true);
        Check(invasion.TryReserveSpawn(true, 2, out var b) &&
            b.NpcClass == "Brute", "weighted brute");
        Check(!invasion.CommitSpawn(true, b.Ticket, "Brute", 3, 10), "duplicate live NPC");
        Check(invasion.CommitSpawn(true, b.Ticket, "Brute", 4, 11), "second NPC");
        Check(!invasion.ReportKill(true, 4, 11, "Scout"), "wrong NPC");
        Check(!invasion.ReportKill(true, 4, 10, "Brute"), "stale generation");
        Check(!invasion.ReportKill(false, 4, 11, "Brute"), "authority kill");
        Check(invasion.ReportKill(true, 4, 11, "Brute") &&
            invasion.Progress == 3, "brute points");
        Check(!invasion.ReportKill(true, 4, 11, "Brute") &&
            invasion.Progress == 3, "no double points");
        Check(invasion.ReportDespawn(true, 3, 10) && invasion.Progress == 3,
            "despawn has no points");
        Check(!invasion.ReportKill(true, 3, 10, "Scout"), "despawn not kill");

        for (int i = 0; i < 3; ++i) invasion.Tick(true, true);
        Check(invasion.TryReserveSpawn(true, int.MaxValue, out var c), "spawn after cooldown");
        Check(invasion.CancelSpawn(true, c.Ticket) && invasion.PendingCount == 0,
            "spawn failure refund");
        Check(!invasion.CancelSpawn(true, c.Ticket), "duplicate cancel");
        for (int i = 0; i < 3; ++i) invasion.Tick(true, true);
        Check(invasion.TryReserveSpawn(true, 1, out var d) &&
            invasion.CommitSpawn(true, d.Ticket, "Brute", 4, 12),
            "slot recycled with new generation");
        Check(invasion.ReportKill(true, 4, 12, "Brute") &&
            invasion.IsComplete && invasion.Progress == 5, "completion capped");
        Check(!invasion.TryReserveSpawn(true, 0, out _) &&
            !invasion.ReportKill(true, 4, 12, "Brute"), "no spawns after victory");

        var complete = invasion.Snapshot();
        Check(complete.Phase == BunnyInvasionPhase.Completed &&
            complete.Active == 0 && complete.Pending == 0, "completed snapshot");
        Check(invasion.Stop(true) && !invasion.Stop(true), "stop idempotent");
        Check(invasion.Restore(true, complete) && invasion.IsComplete,
            "restore victory");
        Check(!invasion.Restore(false, complete), "authority restore");
        Check(!invasion.Restore(true, complete with { Progress = 1 }),
            "reject invalid victory");

        Check(invasion.Start(true), "restart after victory");
        Check(invasion.TryReserveSpawn(true, 0, out var oldTicket) &&
            invasion.CommitSpawn(true, oldTicket.Ticket, "Scout", 6, 25),
            "restart membership");
        var running = invasion.Snapshot();
        Check(invasion.Restore(true, running) &&
            invasion.ActiveCount == 0 && invasion.PendingCount == 0,
            "restore clears untracked entities");
        Check(!invasion.ReportKill(true, 6, 25, "Scout"),
            "no ghost progress after reload");

        invasion.ResetWorld();
        Check(invasion.Phase == BunnyInvasionPhase.Inactive &&
            invasion.Progress == 0 && invasion.ActiveCount == 0 &&
            invasion.PendingCount == 0, "world exit cleanup");
        Check(!invasion.ReportKill(true, 6, 25, "Scout"), "cross-world kill isolated");
        Check(!invasion.Restore(true, new BunnyInvasionSnapshot(
            0, 0, BunnyInvasionPhase.Running, 5, 0, 0, 0)),
            "reject impossible running progress");

        Console.WriteLine("BunnyInvasion: " + passed + " checks OK");
    }

    public static int Main()
    {
        try { Run(); return 0; }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
