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
    }
}
