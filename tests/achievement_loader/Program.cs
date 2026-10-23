using System;
using System.Linq;
using System.Reflection;
using BunnyLoader.Source;

namespace AchievementHost;

public sealed class Boss : BunnyAchievement
{
    public static int Completed;
    public override void SetStaticDefaults() => AddNPCKilledCondition(900);
    public override void OnCompleted() => Completed++;
}

public sealed class ManyWorms : BunnyAchievement
{
    private BunnyAchievementCondition count = null!;
    public override BunnyAchievementCategory Category => BunnyAchievementCategory.Collector;
    public override bool Hidden => count.Value == 0;
    public override void SetStaticDefaults() => count = AddIntCondition(5);
    public override void OnNPCKilled(BunnyAchievementEvent e)
    {
        if (e.ContentType == 10) count.Value++;
    }
}

public sealed class Combo : BunnyAchievement
{
    public override void SetStaticDefaults()
    {
        AddItemCraftCondition(3);
        AddItemPickupCondition(4);
    }
}

internal static class Program
{
    private static int checks;
    private static void Check(bool value)
    {
        if (!value) throw new Exception("Host assertion " + checks);
        checks++;
    }

    private static void Fail<T>(Action run) where T : Exception
    {
        try { run(); }
        catch (T) { checks++; return; }
        throw new Exception("Expected " + typeof(T).Name);
    }

    public static void Main()
    {
        var r = new BunnyAchievementRegistry();
        var a = Assembly.GetExecutingAssembly();
        Check(r.Register("a", "1", a) == 3);
        Check(r.Register("b", "1", a) == 3);
        Check(r.Register("a", "1", a) == 3);
        Check(r.Find("a", "Boss")!.Name == "a/Boss");
        Check(r.Find("b", "Boss")!.Name == "b/Boss");
        Check(r.Find("a", "ManyWorms")!.Hidden);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 900, 1, false)) == 0);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 900)) == 1);
        Check(Boss.Completed == 1);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 900)) == 0);
        for (int i = 0; i < 3; i++) r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 10));
        Check(!r.Find("a", "ManyWorms")!.Hidden);
        var saved = r.Snapshot("a");
        Check(saved.Count == 3);
        Check(!r.Unload("a", "old"));
        Check(r.Register("a", "2", a) == 3);
        Check(r.Find("a", "ManyWorms")!.Hidden);
        Check(r.Restore("a", saved) == 3);
        Check(!r.Find("a", "ManyWorms")!.Hidden);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 10)) == 0);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 10)) == 1);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.ItemCraft, 3)) == 0);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.ItemPickup, 4, 2)) == 1);
        Check(!r.Find("b", "Boss")!.IsCompleted);
        Check(!r.Unload("a", "1"));
        Check(r.Unload("a", "2"));
        Check(r.Find("a", "Boss") == null);
        Check(r.Find("b", "Boss") != null);
        Check(r.Dispatch("a", new(BunnyAchievementEventKind.NpcKilled, 900)) == 0);
        Check(r.Unload("b", "1"));
        Fail<ArgumentException>(() => r.Register("../bad", "1", a));
        Fail<ArgumentException>(() => r.Register("ok", " ", a));
        Fail<ArgumentOutOfRangeException>(() =>
            r.Dispatch("a", new(BunnyAchievementEventKind.ItemPickup, 4, -1)));
        var worm = new ManyWorms();
        worm.SetStaticDefaults();
        Fail<ArgumentOutOfRangeException>(() => worm.Conditions[0].Value = float.NaN);
        Fail<ArgumentOutOfRangeException>(() => worm.Conditions[0].Value = 1.5f);
        Console.WriteLine("AchievementLoader host: " + checks + " checks passed");
    }
}
