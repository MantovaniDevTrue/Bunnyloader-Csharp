using System;
using BunnyLoader.Source;

static class Test
{
    private static int passed;

    private static void Check(bool ok, string name)
    {
        if (!ok) throw new InvalidOperationException(name);
        passed++;
    }

    private static void Advance(BunnyInvasionSpawnPipeline pipe, int ticks)
    {
        for (int i = 0; i < ticks; i++) pipe.Tick(true, true);
    }

    private static int Main()
    {
        var controller = new BunnyInvasionController(new BunnyInvasionDefinition(
            "example", 6, 2, 3, new BunnyInvasionEnemy("Slime", 1, 2)));
        var pipe = new BunnyInvasionSpawnPipeline(controller, 5);
        Check(controller.Start(true), "start");
        Check(pipe.TryReserve(true, 0, out BunnyInvasionSpawn first) &&
            first.Epoch > 0 && first.Ticket > 0, "first ticket");
        Check(!pipe.ReportCreated(false, first.Epoch, first.Ticket, "Slime", 2, 300),
            "client cannot claim");
        Check(!pipe.ReportCreated(true, first.Epoch - 1, first.Ticket, "Slime", 2, 300),
            "other epoch cannot claim");
        Check(!pipe.ReportCreated(true, first.Epoch, first.Ticket, "Zombie", 2, 300),
            "other NPC class cannot claim");
        Check(pipe.ReportCreated(true, first.Epoch, first.Ticket, "Slime", 2, 300),
            "host claims native slot");
        Check(!pipe.ReportCreated(true, first.Epoch, first.Ticket, "Slime", 2, 300),
            "native duplicate claim");
        Check(!pipe.ConfirmFirstTick(true, first.Epoch, "Slime", 2, 1, 301),
            "other native identity not credited");
        Check(!pipe.ConfirmFirstTick(true, first.Epoch, "Slime", 2, 0, 300),
            "missing generation not credited");
        Check(pipe.ConfirmFirstTick(true, first.Epoch, "Slime", 2, 1, 300),
            "first real NPC tick commits");
        Check(!pipe.ConfirmFirstTick(true, first.Epoch, "Slime", 2, 1, 300),
            "double tick cannot commit twice");
        Check(pipe.PendingCount == 0 && controller.ActiveCount == 1 &&
            pipe.ConfirmedCount == 1, "admission bookkeeping");
        Check(controller.ReportKill(true, 2, 1, "Slime") &&
            controller.Progress == 2, "real kill credits points");
        Check(!controller.ReportKill(true, 2, 1, "Slime"),
            "double kill denied");

        Advance(pipe, 3);
        Check(pipe.TryReserve(true, 0, out BunnyInvasionSpawn second),
            "new reservation");
        Check(pipe.ReportCreated(true, second.Epoch, second.Ticket, "Slime", 5, 400),
            "native spawned");
        Advance(pipe, 5);
        Check(pipe.PendingCount == 0 && pipe.ExpiredCount == 1 &&
            controller.PendingCount == 0, "unconfirmed native request expires");
        Check(!pipe.ConfirmFirstTick(true, second.Epoch, "Slime", 5, 2, 400),
            "expired report rejected");

        Check(pipe.TryReserve(true, 0, out BunnyInvasionSpawn third),
            "next wave");
        Check(pipe.ReportFailed(true, third.Epoch, third.Ticket) &&
            pipe.FailedCount == 1 && pipe.PendingCount == 0,
            "failed native spawn cancels reservation");
        Check(!pipe.ReportFailed(true, third.Epoch, third.Ticket),
            "duplicate native failure ignored");

        Check(controller.Stop(true), "stop");
        Check(controller.Start(true), "restart");
        Check(pipe.TryReserve(true, 0, out BunnyInvasionSpawn fourth) &&
            fourth.Epoch != first.Epoch, "new world epoch for restart");
        Check(!pipe.ReportCreated(true, first.Epoch, first.Ticket, "Slime", 2, 300),
            "stale world result rejected");
        Check(pipe.ReportCreated(true, fourth.Epoch, fourth.Ticket, "Slime", 2, 500),
            "valid new session spawn");
        pipe.ResetWorld();
        Check(controller.Phase == BunnyInvasionPhase.Inactive &&
            pipe.PendingCount == 0, "world cleanup");
        Check(!pipe.ConfirmFirstTick(true, fourth.Epoch, "Slime", 2, 1, 500),
            "no admission across worlds");

        Console.WriteLine("Bunny native invasion pipeline: " + passed + " checks OK");
        return 0;
    }
}
