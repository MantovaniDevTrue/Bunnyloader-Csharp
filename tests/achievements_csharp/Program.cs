using BunnyLoader.Source;
using ExampleModCSharp.Achievements;

int checkedCases = 0;
void Check(bool result, string description)
{
    if (!result) throw new InvalidOperationException(description);
    checkedCases++;
    Console.WriteLine("PASS " + description);
}

void Throws<T>(Action action, string description) where T : Exception
{
    try { action(); }
    catch (T) { Check(true, description); return; }
    throw new InvalidOperationException(description);
}

var registry = new BunnyAchievementRegistry();
var boss = registry.Register("examplemod", new ExampleBossKilled());
var worms = registry.Register("examplemod", new ManyWormsKilled());
Check(registry.Achievements.Count == 2 && boss.Name == "examplemod/ExampleBossKilled",
    "stable identities and registry");
Check(!worms.IsCompleted && worms.Hidden && worms.Progress == 0, "hidden before first kill");
registry.NotifyNPCKilled(10, localPlayer: false);
Check(worms.Progress == 0, "remote kill is ignored");
registry.NotifyNPCKilled(10);
Check(!worms.Hidden && Math.Abs(worms.Progress - .2) < .001, "numeric progress");
registry.NotifyNPCKilled(1234);
Check(boss.IsCompleted && !worms.IsCompleted, "automatic NPC kill condition");
for (int i = 0; i < 4; i++) registry.NotifyNPCKilled(10);
Check(worms.IsCompleted && worms.Progress == 1, "custom counter completes exactly at five");
registry.NotifyNPCKilled(10);
Check(worms.Conditions[0].Value == 5, "no overflow on completed conditions");

string snapshot = registry.Save();
var restored = new BunnyAchievementRegistry();
restored.Load(snapshot);
var recoveredBoss = restored.Register("examplemod", new ExampleBossKilled());
var recoveredWorms = restored.Register("examplemod", new ManyWormsKilled());
Check(recoveredBoss.IsCompleted && recoveredWorms.IsCompleted, "restore before registration");
Check(restored.Unregister(recoveredWorms.Name) &&
    restored.Register("examplemod", new ManyWormsKilled()).IsCompleted, "preserve progress on unload");
restored.Load("BACH1\n");
Check(!recoveredBoss.IsCompleted && !recoveredBoss.Conditions[0].IsCompleted, "clear progress with empty save");
Throws<FormatException>(() => restored.Load("BACH1\nGG|AB|1\n"), "reject malformed hex");
Throws<FormatException>(() => restored.Load("BACH1\n6162|6162|NaN\n"), "reject NaN");
Throws<FormatException>(() => restored.Load("BACH1\n6162|6162|-1\n"), "reject negative progress");
Throws<InvalidOperationException>(() => restored.Register("examplemod", new ExampleBossKilled()),
    "reject duplicate identities");

var conditions = new BunnyAchievementRegistry();
var duo = conditions.Register("multi", new TwoConditions());
conditions.NotifyItemPickup(222, 1);
Check(!duo.IsCompleted && Math.Abs(duo.Progress - .5) < .001, "multiple conditions require all");
conditions.NotifyItemCraft(333, 1);
Check(duo.IsCompleted && duo.CompletedCalls == 1, "one completion callback");
conditions.NotifyItemCraft(333, 1);
Check(duo.CompletedCalls == 1, "completion callback is not duplicated");
var save = conditions.Save();
var duplicate = new BunnyAchievementRegistry();
duplicate.Load(save);
var duoRestored = duplicate.Register("multi", new TwoConditions());
Check(duoRestored.IsCompleted && duoRestored.CompletedCalls == 0, "restore does not display unlock twice");

var faultRegistry = new BunnyAchievementRegistry();
faultRegistry.Register("fault", new FailingAchievement());
var independent = faultRegistry.Register("independent", new ExampleBossKilled());
faultRegistry.NotifyNPCKilled(1234);
Check(faultRegistry.CallbackFaults == 1 && independent.IsCompleted, "callback fault isolation");
Throws<ArgumentException>(() => faultRegistry.Register("bad/uid", new ExampleBossKilled()), "invalid UID");

Console.WriteLine("BUNNY_CSHARP_ACHIEVEMENTS_CORE=PASS checks=" + checkedCases);

sealed class TwoConditions : BunnyAchievement
{
    public int CompletedCalls { get; private set; }

    public override void SetStaticDefaults()
    {
        Texture = "Content/Achievements/TwoConditions.png";
        AddItemPickupCondition(222);
        AddItemCraftCondition(333);
    }

    public override void OnCompleted() => CompletedCalls++;
}

sealed class FailingAchievement : BunnyAchievement
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Achievements/Failing.png";
        AddIntCondition("AnyKill", 5);
    }

    public override void OnNPCKilled(int npcType) => throw new InvalidOperationException("expected");
}
