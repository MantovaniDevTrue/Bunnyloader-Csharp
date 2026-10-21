using System;
using System.Collections.Generic;
using BunnyLoader.Source;
using ExampleModCSharp;

internal static class DispatcherTests
{
    private const string A = "00000000-0000-4000-8000-000000000001";
    private const string B = "00000000-0000-4000-8000-000000000002";

    private static void Check(bool result, string name)
    {
        if (!result) throw new Exception("FAIL " + name);
        Console.WriteLine("PASS " + name);
    }

    public static void Run()
    {
        var untouched = new BunnyWorldDocument();
        var data = new BunnyWorldTag();
        data.SetBoolean("inactiveModFlag", true);
        untouched.Replace(B, "InactiveMod.Progress", data);

        var events = new List<string>();
        var system = new ExampleDownedBossWorldSystem();
        var tracked = new TrackingWorldSystem(events);
        var dispatcher = new BunnyWorldDispatcher();
        Check(dispatcher.TryRegister(A, system, out _), "register example");
        Check(dispatcher.TryRegister(A, tracked, out _), "register tracker");
        Check(!dispatcher.TryRegister(A, tracked, out _) &&
              dispatcher.RegisteredSystems == 2, "duplicate registration rejected");
        Check(dispatcher.TryEnterWorld(42, untouched.Encode(), true, out _),
              "load world with missing mod");
        Check(events.Count == 3 && events[0] == "Clear" &&
              events[1] == "Load" && events[2] == "Post",
              "callback order");
        system.MarkExampleBossDefeated();
        Check(dispatcher.TryPrepareSave(42, out var snapshot, out _), "prepare save");
        Check(BunnyWorldDocument.TryDecode(snapshot, out var restored), "decode saved state");
        Check(restored.Read(B, "InactiveMod.Progress").GetBoolean("inactiveModFlag"),
              "disabled mod data preserved");
        Check(restored.Read(A, typeof(ExampleDownedBossWorldSystem).FullName!)
              .GetBoolean("downedExampleBoss"), "example boss flag saved");
        Check(dispatcher.TryConfirmSaved(42, snapshot), "confirm after disk commit");
        Check(!dispatcher.TryConfirmSaved(999, snapshot), "reject late commit");
        Check(!dispatcher.TryPrepareSave(41, out _, out _), "reject old epoch");
        Check(dispatcher.TryLeaveWorld(42, out _) &&
              events[events.Count - 1] == "Unload", "unload called");

        Check(dispatcher.TryEnterWorld(43, null, true, out _), "new world");
        Check(!system.DownedExampleBoss, "new world clears boss");
        Check(dispatcher.TryPrepareSave(43, out snapshot, out _), "new world saved");
        Check(BunnyWorldDocument.TryDecode(snapshot, out restored) &&
              !restored.Read(A, typeof(ExampleDownedBossWorldSystem).FullName!)
                  .GetBoolean("downedExampleBoss"), "new world has no stale flag");
        Check(dispatcher.TryLeaveWorld(43, out _), "second world unload");

        byte[] broken = untouched.Encode();
        broken[5] ^= 1;
        Check(!dispatcher.TryEnterWorld(44, broken, true, out _),
              "corrupt save rejected");
        Check(!dispatcher.TryPrepareSave(44, out _, out _),
              "corrupt save cannot be overwritten");
        Check(!dispatcher.WorldActive, "corrupt world automatically unloaded");
        Check(dispatcher.TryEnterWorld(45, null, false, out _),
              "client loads callbacks");

        Check(!dispatcher.TryPrepareSave(45, out _, out _),
              "client cannot persist world data");
        Check(dispatcher.TryLeaveWorld(45, out _), "client cleanup");

        var failing = new BunnyWorldDispatcher();
        Check(failing.TryRegister(A, new ThrowingSaveSystem(), out _), "register faulty system");
        Check(failing.TryEnterWorld(88, null, true, out _), "fault system loads");
        Check(!failing.TryPrepareSave(88, out var rejected, out var reason) &&
              rejected.Length == 0 && reason.Contains("SaveWorldData"),
              "callback exception blocks whole save");
        Check(failing.TryLeaveWorld(88, out _), "fault system cleanup");
        var brokenLoad = new BunnyWorldDispatcher();
        Check(brokenLoad.TryRegister(A, new ThrowingLoadSystem(), out _),
              "register failing load system");
        Check(!brokenLoad.TryEnterWorld(90, null, true, out var loadError) &&
              loadError.Contains("OnWorldLoad") && !brokenLoad.WorldActive,
              "failed world entry rolls back");
        Check(brokenLoad.TryEnterWorld(91, null, true, out _) == false &&
              !brokenLoad.WorldActive, "reentry after callback error is not locked");
        Console.WriteLine("WORLD_DISPATCHER_TESTS_PASS");
    }
}

public sealed class TrackingWorldSystem : BunnyWorldSystem
{
    public static List<string>? Events;
    public TrackingWorldSystem() { }
    public TrackingWorldSystem(List<string> events) { Events = events; }
    public override void ClearWorld() => Events?.Add("Clear");
    public override void OnWorldLoad() => Events?.Add("Load");
    public override void PostWorldLoad() => Events?.Add("Post");
    public override void OnWorldUnload() => Events?.Add("Unload");
}

public sealed class ThrowingSaveSystem : BunnyWorldSystem
{
    public override void SaveWorldData(BunnyWorldTag tag) =>
        throw new InvalidOperationException("test simulated failure");
}

public sealed class ThrowingLoadSystem : BunnyWorldSystem
{
    public override void OnWorldLoad() =>
        throw new InvalidOperationException("test load failure");
}
