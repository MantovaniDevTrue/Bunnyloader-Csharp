using System;
using System.Reflection;
using BunnyLoader.Source;

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
        Console.WriteLine("Bunny World Systems smoke: 17 checks passed");
        return 0;
    }
}
