using System;
using System.Reflection;
using BunnyLoader.Source;
using ExampleModCSharp;

namespace BunnyWorldSystemsSmoke;

public sealed class AlphaWorld : BunnyWorldSystem
{
    public static int Enters;
    public static int Exits;
    public static int Ticks;
    public static long Frames;

    public override void OnWorldEnter(in BunnyWorldContext world)
    {
        Enters++;
        if (world.ElapsedFrames != 30) throw new InvalidOperationException("Enter frames");
    }

    public override void OnWorldTick(in BunnyWorldContext world)
    {
        Ticks++;
        Frames += world.ElapsedFrames;
    }

    public override void OnWorldExit()
    {
        Exits++;
    }
}

public sealed class BrokenWorld : BunnyWorldSystem
{
    public static int Tries;
    public static int Exits;

    public override void OnWorldTick(in BunnyWorldContext world)
    {
        Tries++;
        throw new InvalidOperationException("Expected isolated test fault");
    }

    public override void OnWorldExit()
    {
        Exits++;
    }
}

internal static class Program
{
    private static void Check(bool passed, string name)
    {
        if (!passed) throw new InvalidOperationException(name);
        Console.WriteLine("PASS " + name);
    }

    public static int Main()
    {
        string assemblyPath = Assembly.GetExecutingAssembly().Location;
        int mod = BunnyEntitySourceDispatcher.Register("world-system-smoke", assemblyPath);
        Check(mod >= 0, "Register");
        Check(BunnyEntitySourceDispatcher.Register("world-system-smoke", assemblyPath) == mod,
            "Idempotent registration");

        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 10, 0, 32, 64);
        Check(AlphaWorld.Enters == 1 && AlphaWorld.Ticks == 1 && AlphaWorld.Frames == 30,
            "Enter and first world tick");
        Check(BrokenWorld.Tries == 1 && BrokenWorld.Exits == 0, "Fault is isolated");

        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 10, 0, 32, 64);
        Check(AlphaWorld.Ticks == 2 && BrokenWorld.Tries == 1, "Healthy systems continue");
        Check(BunnyEntitySourceDispatcher.Status(mod).Contains("faults=1"),
            "Fault is reported");

        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 11, 0, 32, 64);
        Check(AlphaWorld.Enters == 2 && AlphaWorld.Exits == 1 && AlphaWorld.Ticks == 3,
            "Session transition");
        Check(BrokenWorld.Tries == 2 && BrokenWorld.Exits == 1,
            "Failed system rearmed in new session");

        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 10, 0, 32, 64);
        Check(AlphaWorld.Ticks == 3, "Stale session is rejected");

        BunnyEntitySourceDispatcher.ResetWorld();
        Check(AlphaWorld.Exits == 2 && BrokenWorld.Exits == 2, "World reset cleanup");

        BunnyEntitySourceDispatcher.TickWorld(mod, 0, 30, 12, 0, 32, 64);
        Check(AlphaWorld.Enters == 3 && AlphaWorld.Ticks == 4,
            "World can reenter after reset");
        var worldTag = new BunnyWorldData();
        worldTag.SetBool("boss", true);
        worldTag.SetInt("score", -42);
        worldTag.SetString("title", "Exemplo 茶");
        Check(worldTag.ContainsKey("boss") && worldTag.GetBool("boss") &&
            worldTag.GetInt("score") == -42 && worldTag.GetString("title") == "Exemplo 茶",
            "Typed world data");
        Check(!worldTag.GetBool("unknown") && worldTag.GetInt("unknown", 7) == 7,
            "World data defaults");

        var encode = typeof(BunnyWorldData).GetMethod("Encode",
            BindingFlags.Instance | BindingFlags.NonPublic);
        var decode = typeof(BunnyWorldData).GetMethod("TryDecode",
            BindingFlags.Static | BindingFlags.NonPublic);
        string serialized = (string)(encode!.Invoke(worldTag, null) ??
            throw new InvalidOperationException("Encode missing"));
        object?[] arguments = { serialized, null };
        Check((bool)(decode!.Invoke(null, arguments) ?? false), "World data decode");
        var restored = (BunnyWorldData)arguments[1]!;
        Check(restored.GetBool("boss") && restored.GetInt("score") == -42 &&
            restored.GetString("title") == "Exemplo 茶", "World data roundtrip");
        object?[] broken = { "bunny-world-data 1\\nboss\\tb1\\nboss\\tb0\\n", null };
        Check(!(bool)(decode.Invoke(null, broken) ?? true), "Reject duplicate world data");
        bool keyRejected = false;
        try { worldTag.SetBool("../bad", true); }
        catch (ArgumentException) { keyRejected = true; }
        Check(keyRejected, "Reject unsafe world data key");
        var writer = new BunnyNetWriter();
        writer.WriteFlags(true, false, true);
        writer.WriteInt32(-123);
        writer.WriteString("Bunny 🐰");
        var reader = new BunnyNetReader(writer.ToArray());
        bool[] flags = reader.ReadFlags();
        Check(flags.Length == 3 && flags[0] && !flags[1] && flags[2],
            "NetSend flags");
        Check(reader.ReadInt32() == -123 && reader.ReadString() == "Bunny 🐰" &&
            reader.Remaining == 0, "NetReceive typed payload");
        var packet = new BunnyWorldNetState(71, "mod_one", "Progress", 1,
            writer.ToArray());
        Check(packet.IsValid() &&
            BunnyWorldNetState.TryDecode(packet.Encode(), out BunnyWorldNetState roundtrip) &&
            roundtrip.Revision == 1 && roundtrip.Data.Length == writer.Length,
            "World sync encode decode");
        var view = new BunnyWorldNetClientView();
        Check(view.Accept(packet, 5, "mod_one", "Progress") &&
            !view.Accept(packet, 5, "mod_one", "Progress"), "Reject replayed revision");
        Check(!view.Accept(packet with { Revision = 2, HostSession = 72 },
            5, "mod_one", "Progress"), "Reject remote world switch");
        Check(view.Accept(packet with { Revision = 2 }, 5, "mod_one", "Progress"),
            "Accept newer revision");
        view.Reset();
        Check(view.Accept(packet, 6, "mod_one", "Progress"),
            "Accept new local world");
        Check(!BunnyWorldNetState.TryDecode(
            "WS1|71|mod_one|Progress|1|GG", out _), "Reject malformed wire packet");
        bool truncated = false;
        try { new BunnyNetReader(new byte[] { 3, 1 }).ReadFlags(); }
        catch (FormatException) { truncated = true; }
        Check(!truncated, "Packed 3 flags fit one byte");
        truncated = false;
        try { new BunnyNetReader(new byte[] { 9, 1 }).ReadFlags(); }
        catch (FormatException) { truncated = true; }
        Check(truncated, "Reject truncated flags");
        bool exceeded = false;
        try { new BunnyNetWriter().WriteString(new string('x', 65)); }
        catch (ArgumentOutOfRangeException) { exceeded = true; }
        Check(exceeded, "Reject network string capacity");
        Check(!view.Accept(packet, 6, "other_mod", "Progress"),
            "Reject mismatched mod identity");
        var bossSystem = new ExampleWorldProgressSystem();
        bossSystem.ClearWorld();
        Check(!ExampleWorldProgressSystem.DownedExampleBoss,
            "ExampleMod boss progression clears on world entry");
        ExampleWorldProgressSystem.MarkBossDefeated();
        var bossTag = new BunnyWorldData();
        bossSystem.SaveWorldData(bossTag);
        bossSystem.ClearWorld();
        bossSystem.LoadWorldData(bossTag);
        Check(ExampleWorldProgressSystem.DownedExampleBoss,
            "ExampleMod boss defeat survives save/load");
        var bossWriter = new BunnyNetWriter();
        bossSystem.NetSend(bossWriter);
        bossSystem.ClearWorld();
        var bossReader = new BunnyNetReader(bossWriter.ToArray());
        bossSystem.NetReceive(bossReader);
        Check(ExampleWorldProgressSystem.DownedExampleBoss &&
              bossReader.Remaining == 0,
            "ExampleMod boss progression survives host/client network roundtrip");
        var invalidBossPacket = new BunnyNetWriter();
        invalidBossPacket.WriteFlags(true, false);
        bool rejectedBossPacket = false;
        try { bossSystem.NetReceive(new BunnyNetReader(invalidBossPacket.ToArray())); }
        catch (FormatException) { rejectedBossPacket = true; }
        Check(rejectedBossPacket, "ExampleMod rejects invalid network flag count");
        var trailingBossPacket = new BunnyNetWriter();
        trailingBossPacket.WriteFlags(false);
        trailingBossPacket.WriteByte(42);
        bool rejectedTrailingBytes = false;
        try { bossSystem.NetReceive(new BunnyNetReader(trailingBossPacket.ToArray())); }
        catch (FormatException) { rejectedTrailingBytes = true; }
        Check(rejectedTrailingBytes && ExampleWorldProgressSystem.DownedExampleBoss,
            "Malformed packet cannot alter boss progression");
        bossSystem.ClearWorld();
        var clearedWriter = new BunnyNetWriter();
        bossSystem.NetSend(clearedWriter);
        ExampleWorldProgressSystem.MarkBossDefeated();
        bossSystem.NetReceive(new BunnyNetReader(clearedWriter.ToArray()));
        Check(!ExampleWorldProgressSystem.DownedExampleBoss,
            "Host progress reset replicates to the client");
        var normalPacket = new BunnyWorldNetState(1, "mod", "System", 1,
            new byte[BunnyNetWriter.MaxBytes]);
        Check(normalPacket.IsValid() &&
            normalPacket.Encode().Length <= 300, "Network full payload budget");
        var longPacket = new BunnyWorldNetState(long.MaxValue,
            new string('m', 110), new string('s', 60), long.MaxValue,
            new byte[BunnyNetWriter.MaxBytes]);
        Check(!longPacket.IsValid() &&
            longPacket.Encode().Length > 300,
            "Reject overbudget world packet without encoding");
        var borderlinePacket = new BunnyWorldNetState(long.MaxValue,
            new string('m', 40), new string('s', 30), long.MaxValue,
            new byte[BunnyNetWriter.MaxBytes]);
        Check(borderlinePacket.IsValid() ==
            (borderlinePacket.Encode().Length <= 300),
            "Encoded length exactly matches network budget");
        var minPacket = new BunnyWorldNetState(1, "m", "s", 1, new byte[] { 0 });
        Check(minPacket.IsValid() &&
            minPacket.Encode().Length == 8 + 1 + 1 + 1 + 1 + 2,
            "Minimum world packet length");
        var boundaryPacket = new BunnyWorldNetState(1, new string('m', 42),
            new string('s', 42), 1, new byte[BunnyNetWriter.MaxBytes]);
        Check(boundaryPacket.IsValid() ==
            (boundaryPacket.Encode().Length <= 300),
            "Boundary-sized world packet budget");
        bool invalidCount = false;
        try { new BunnyNetReader(new byte[] { 0 }).ReadFlags(); }
        catch (FormatException) { invalidCount = true; }
        Check(invalidCount, "Reject zero network flags");

        invalidCount = false;
        try { new BunnyNetReader(new byte[] { 17, 0, 0, 0 }).ReadFlags(); }
        catch (FormatException) { invalidCount = true; }
        Check(invalidCount, "Reject overlong network flags");

        bool invalidPadding = false;
        try { new BunnyNetReader(new byte[] { 3, 0x80 }).ReadFlags(); }
        catch (FormatException) { invalidPadding = true; }
        Check(invalidPadding, "Reject nonzero packed flag padding");

        bool invalidText = false;
        try { new BunnyNetReader(new byte[] { 1, 0xFF }).ReadString(); }
        catch (System.Text.DecoderFallbackException) { invalidText = true; }
        Check(invalidText, "Reject invalid UTF8 network strings");

        bool truncatedInteger = false;
        try { new BunnyNetReader(new byte[] { 1, 2, 3 }).ReadInt32(); }
        catch (FormatException) { truncatedInteger = true; }
        Check(truncatedInteger, "Reject incomplete network integers");

        bool utf8Overflow = false;
        try { new BunnyNetWriter().WriteString(new string('é', 33)); }
        catch (ArgumentOutOfRangeException) { utf8Overflow = true; }
        Check(utf8Overflow, "Bound UTF8 strings by bytes not chars");

        bool writerOverflow = false;
        var fullWriter = new BunnyNetWriter();
        for (int n = 0; n < 24; n++) fullWriter.WriteInt32(n);
        try { fullWriter.WriteByte(1); }
        catch (InvalidOperationException) { writerOverflow = true; }
        Check(writerOverflow && fullWriter.Length == BunnyNetWriter.MaxBytes,
            "Refuse overcapacity writes without modifying buffer");

        bool payloadOverflow = false;
        try { new BunnyNetReader(new byte[BunnyNetWriter.MaxBytes + 1]); }
        catch (ArgumentOutOfRangeException) { payloadOverflow = true; }
        Check(payloadOverflow, "Reject oversized received buffers");
        Console.WriteLine("Bunny World Systems smoke: 48 checks passed");
        return 0;
    }
}
