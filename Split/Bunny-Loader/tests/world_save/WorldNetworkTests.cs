using System;
using System.IO;
using BunnyLoader.Source;
using ExampleModCSharp;

internal static class WorldNetworkTests
{
    public static void Run()
    {
        var server = new ExampleDownedBossWorldSystem();
        var client = new ExampleDownedBossWorldSystem();
        server.MarkExampleBossDefeated();
        using var wire = new MemoryStream();
        using (var writer = new BinaryWriter(wire, System.Text.Encoding.UTF8, true))
            server.NetSend(writer);
        if (wire.Length != 1) throw new Exception("BunnyWorldSystem NetSend invalid length");
        wire.Position = 0;
        using (var reader = new BinaryReader(wire, System.Text.Encoding.UTF8, true))
            client.NetReceive(reader);
        if (!client.DownedExampleBoss)
            throw new Exception("BunnyWorldSystem NetReceive missed server progress");
        server.ClearWorld();
        wire.SetLength(0);
        wire.Position = 0;
        using (var writer = new BinaryWriter(wire, System.Text.Encoding.UTF8, true))
            server.NetSend(writer);
        wire.Position = 0;
        using (var reader = new BinaryReader(wire, System.Text.Encoding.UTF8, true))
            client.NetReceive(reader);
        if (client.DownedExampleBoss)
            throw new Exception("BunnyWorldSystem NetReceive kept stale boss flag");
        var systems = new Dictionary<string, BunnyWorldSystem> {
            [BunnyWorldDocument.MakeKey("00000000-0000-4000-8000-000000000001",
                typeof(ExampleDownedBossWorldSystem).FullName!)] = server
        };
        byte[] packet = BunnyWorldNetworkCodec.Encode(systems, 1);
        if (!BunnyWorldNetworkCodec.TryDecode(packet, out var revision, out var decoded) ||
            revision != 1 || decoded.Count != 1)
            throw new Exception("BWN1 network roundtrip failed");
        byte[] invalid = (byte[])packet.Clone();
        invalid[invalid.Length - 1] ^= 1;
        if (BunnyWorldNetworkCodec.TryDecode(invalid, out _, out _) ||
            BunnyWorldNetworkCodec.TryDecode(packet.AsSpan(0, packet.Length - 2), out _, out _))
            throw new Exception("BWN1 corruption not rejected");
        if (BunnyWorldNetworkCodec.TryDecode(new byte[70000], out _, out _))
            throw new Exception("BWN1 oversized packet accepted");
        Console.WriteLine("WORLD_NET_CALLBACKS_PASS");
        Console.WriteLine("WORLD_NET_CODEC_PASS");
        // Simula servidor e cliente com epochs locais diferentes.
        var authoritativeBoss = new ExampleDownedBossWorldSystem();
        var clientBoss = new ExampleDownedBossWorldSystem();
        var authoritative = new BunnyWorldDispatcher();
        var remote = new BunnyWorldDispatcher();
        const string modId = "00000000-0000-4000-8000-000000000001";
        if (!authoritative.TryRegister(modId, authoritativeBoss, out _) ||
            !remote.TryRegister(modId, clientBoss, out _) ||
            !authoritative.TryEnterWorld(123, null, true, out _) ||
            !remote.TryEnterWorld(7, null, false, out _))
            throw new Exception("Network dispatcher setup failed");
        authoritativeBoss.MarkExampleBossDefeated();
        if (!authoritative.TryBuildNetworkState(123, out var firstPacket, out _) ||
            !remote.TryApplyNetworkState(7, firstPacket, out _) ||
            !clientBoss.DownedExampleBoss)
            throw new Exception("Authoritative boss sync failed");
        if (remote.TryApplyNetworkState(7, firstPacket, out _) ||
            remote.TryBuildNetworkState(7, out _, out _) ||
            authoritative.TryApplyNetworkState(123, firstPacket, out _))
            throw new Exception("Network permissions or replay failed");
        authoritativeBoss.ClearWorld();
        if (!authoritative.TryBuildNetworkState(123, out var nextPacket, out _) ||
            !remote.TryApplyNetworkState(7, nextPacket, out _) ||
            clientBoss.DownedExampleBoss)
            throw new Exception("Revised world state not applied");
        if (!remote.TryLeaveWorld(7, out _) ||
            !authoritative.TryLeaveWorld(123, out _) ||
            !remote.TryEnterWorld(8, null, false, out _) ||
            !remote.TryApplyNetworkState(8, firstPacket, out _) ||
            !clientBoss.DownedExampleBoss ||
            !remote.TryLeaveWorld(8, out _))
            throw new Exception("Network epoch reset failed");
        Console.WriteLine("WORLD_NET_DISPATCHER_PASS");

    }
}
