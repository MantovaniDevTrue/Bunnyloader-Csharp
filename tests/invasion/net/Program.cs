using BunnyLoader.Source;

static class Tests
{
    static int passed;
    static void Check(bool condition, string test)
    {
        if (!condition) throw new InvalidOperationException(test);
        passed++;
    }

    public static int Main()
    {
        var controller = new BunnyInvasionController(new BunnyInvasionDefinition(
            "invasion-a", 10, 3, 30, new BunnyInvasionEnemy("Slime", 1, 2)));
        var view = new BunnyInvasionClientView();
        Check(controller.Start(true), "host starts");
        var state = BunnyInvasionWireState.Capture(11, "my_mod", controller);
        Check(state.IsValid(), "snapshot valid");
        Check(BunnyInvasionWireState.TryDecode(state.Encode(), out var decoded) &&
            decoded.Equals(state), "wire roundtrip");
        Check(view.ReceiveFromHost(decoded, 11, "my_mod", "invasion-a"),
            "client accepts authoritative snapshot");
        Check(!view.ReceiveFromHost(decoded, 11, "my_mod", "invasion-a"),
            "duplicate rejected");
        Check(!view.ReceiveFromHost(decoded with { Revision = decoded.Revision - 1 },
            11, "my_mod", "invasion-a"), "older revision rejected");
        Check(!view.ReceiveFromHost(decoded, 12, "my_mod", "invasion-a"),
            "wrong world rejected");
        Check(!view.ReceiveFromHost(decoded, 11, "other-mod", "invasion-a"),
            "wrong mod rejected");
        Check(!view.ReceiveFromHost(decoded, 11, "my_mod", "other-event"),
            "wrong event rejected");
        Check(!BunnyInvasionWireState.TryDecode(decoded.Encode() + "|oops", out _),
            "extra fields rejected");
        Check(!BunnyInvasionWireState.TryDecode("INV2|" + decoded.Encode(), out _),
            "unknown protocol rejected");
        Check(!BunnyInvasionWireState.TryDecode(decoded.Encode().Replace("my_mod", "../bad"),
            out _), "path-like IDs rejected");
        Check(!BunnyInvasionWireState.TryDecode(decoded.Encode().Replace("|10|", "|-1|"),
            out _), "negative or malformed data rejected");
        Check(!BunnyInvasionWireState.TryDecode(new string('a', 302), out _),
            "oversized packet rejected");

        Check(controller.TryReserveSpawn(true, 0, out var spawn), "host queues NPC");
        Check(controller.CommitSpawn(true, spawn.Ticket, "Slime", 1, 44),
            "host admits NPC");
        Check(controller.ReportKill(true, 1, 44, "Slime"), "host kill");
        state = BunnyInvasionWireState.Capture(11, "my_mod", controller);
        Check(view.ReceiveFromHost(state, 11, "my_mod", "invasion-a") &&
            view.Current.Progress == 2, "newer progress accepted");
        Check(!view.ReceiveFromHost(decoded, 11, "my_mod", "invasion-a"),
            "out-of-order old progress rejected");
        var late = new BunnyInvasionClientView();
        Check(late.ReceiveFromHost(state, 11, "my_mod", "invasion-a") &&
            late.Current.Progress == 2, "late join receives complete snapshot");
        view.ResetWorld();
        Check(!view.HasState &&
            view.ReceiveFromHost(state with { WorldSession = 22 }, 22, "my_mod", "invasion-a"),
            "world reset releases stale epoch");
        var other = new BunnyInvasionClientView();
        Check(other.ReceiveRemoteFromHost(state with { WorldSession = 1099 }, 22,
            "my_mod", "invasion-a"), "remote host session need not equal local world session");
        Check(!other.ReceiveRemoteFromHost(state with { WorldSession = 1100 }, 22,
            "my_mod", "invasion-a"), "session hijack ignored in same local world");
        Check(other.ReceiveRemoteFromHost(state with { WorldSession = 1100 }, 23,
            "my_mod", "invasion-a"), "new local world permits new host session");
        Check(!other.ReceiveRemoteFromHost(state with { WorldSession = 1099 }, 23,
            "my_mod", "invasion-a"), "old server session ignored after rejoin");
        var malformed = state with { Phase = BunnyInvasionPhase.Completed };
        Check(!malformed.IsValid(), "cannot broadcast incomplete finish");
        Console.WriteLine("Invasion host/client replication protocol: " + passed + " checks OK");
        return 0;
    }
}
