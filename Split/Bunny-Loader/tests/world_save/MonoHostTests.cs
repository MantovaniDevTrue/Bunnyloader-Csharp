using System;
using BunnyLoader.Source;

internal static class MonoHostTests
{
    private const string Mod = "00000000-0000-4000-8000-000000000001";
    private const string Missing = "00000000-0000-4000-8000-000000000002";
    private const string ClassName = "ExampleModCSharp.ExampleDownedBossWorldSystem";

    private static void Check(bool ok, string name)
    {
        if (!ok) throw new InvalidOperationException("FAILED " + name +
            " " + BunnyWorldMonoHost.LastError);
        Console.WriteLine("PASS " + name);
    }

    public static void Run(string exampleDll)
    {
        Check(BunnyWorldMonoHost.ResetRegistrations(), "host reset");
        Check(!BunnyWorldMonoHost.RegisterMod(Mod, "../world.dll"), "no relative DLL");
        Check(BunnyWorldMonoHost.RegisterMod(Mod, exampleDll), "discover ExampleMod class");
        Check(BunnyWorldMonoHost.RegisteredSystems == 1, "one world system discovered");
        Check(!BunnyWorldMonoHost.RegisterMod(Mod, exampleDll), "duplicate class rejected");

        var sidecar = new BunnyWorldDocument();
        var old = new BunnyWorldTag();
        old.SetBoolean("downedExampleBoss", true);
        sidecar.Replace(Mod, ClassName, old);
        var absent = new BunnyWorldTag();
        absent.SetInt32("preserved", 55);
        sidecar.Replace(Missing, "Unloaded.Progress", absent);

        Check(!BunnyWorldMonoHost.Enter(1, "@invalid@", true), "bad Base64 rejected");
        Check(!BunnyWorldMonoHost.WorldActive, "malformed data does not start world");
        Check(BunnyWorldMonoHost.Enter(12,
            Convert.ToBase64String(sidecar.Encode()), true), "enter world from bytes");
        Check(!BunnyWorldMonoHost.RegisterMod(Missing, exampleDll),
            "register prohibited in world");
        string? snapshot = BunnyWorldMonoHost.Prepare(12);
        Check(snapshot != null, "managed callbacks produced snapshot");
        Check(BunnyWorldDocument.TryDecode(Convert.FromBase64String(snapshot!), out var decoded),
            "snapshot is valid BWC1");
        Check(decoded.Read(Mod, ClassName).GetBoolean("downedExampleBoss"),
            "example progression restored");
        Check(decoded.Read(Missing, "Unloaded.Progress").GetInt32("preserved") == 55,
            "other mod preserved");
        Check(!BunnyWorldMonoHost.Confirm(11, snapshot!), "stale acknowledgment rejected");
        Check(BunnyWorldMonoHost.Confirm(12, snapshot!), "save acknowledged");
        string? network = BunnyWorldMonoHost.NetSend(12);
        Check(network != null && BunnyWorldNetworkCodec.TryDecode(
            Convert.FromBase64String(network), out _, out _),
            "managed MonoVM network export");
        Check(!BunnyWorldMonoHost.NetReceive(12, network),
            "server rejects network import");

        Check(!BunnyWorldMonoHost.ResetRegistrations(), "no live reset");
        Check(BunnyWorldMonoHost.Leave(12), "world unload");
        Check(BunnyWorldMonoHost.Enter(13, null, true), "enter fresh world");
        snapshot = BunnyWorldMonoHost.Prepare(13);
        Check(snapshot != null, "new world save");
        Check(BunnyWorldDocument.TryDecode(Convert.FromBase64String(snapshot!), out decoded) &&
              !decoded.Read(Mod, ClassName).GetBoolean("downedExampleBoss"),
              "new world flag cleared");
        Check(BunnyWorldMonoHost.Leave(13), "leave fresh world");

        Check(BunnyWorldMonoHost.Enter(14, null, false), "client entry");
        Check(network != null && BunnyWorldMonoHost.NetReceive(14, network),
              "client imported network world state");
        Check(BunnyWorldMonoHost.NetSend(14) == null, "client cannot export authority");
        Check(!BunnyWorldMonoHost.NetReceive(14, network),
              "duplicate network revision rejected");

        Check(BunnyWorldMonoHost.Prepare(14) == null, "multiplayer client cannot write");
        Check(BunnyWorldMonoHost.Leave(14), "client exit");

        var corrupted = Convert.ToBase64String(new byte[] { 1, 2, 3, 4 });
        Check(!BunnyWorldMonoHost.Enter(15, corrupted, true), "invalid BWC1 rejected");
        Check(BunnyWorldMonoHost.Prepare(15) == null, "corruption disables save");
        Check(BunnyWorldMonoHost.Leave(15), "invalid session cleaned");
        Check(BunnyWorldMonoHost.ResetRegistrations() &&
              BunnyWorldMonoHost.RegisteredSystems == 0, "unload clears registrations");
        Console.WriteLine("WORLD_MONO_HOST_TESTS_PASS");
    }
}
