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
        Console.WriteLine("WORLD_NET_CALLBACKS_PASS");
    }
}
