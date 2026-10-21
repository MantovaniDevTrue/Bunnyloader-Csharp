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
long completionRevision = registry.ProgressChanges;
worms.Conditions[0].Value = 0;
Check(worms.IsCompleted && worms.Conditions[0].Value == 5 &&
    registry.ProgressChanges == completionRevision,
    "unlocked achievement cannot regress or publish spurious progress");

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

var negatives = new BunnyAchievementRegistry();
var negativeNpc = negatives.Register("variants", new NegativeNpcAchievement());
negatives.NotifyNPCKilled(-4);
Check(negativeNpc.IsCompleted, "negative vanilla NPC netID dispatch preserves variants");
negatives.NotifyItemPickup(-4, 1);
Check(negativeNpc.IsCompleted, "negative item type ignored");

var faultRegistry = new BunnyAchievementRegistry();
faultRegistry.Register("fault", new FailingAchievement());
var independent = faultRegistry.Register("independent", new ExampleBossKilled());
faultRegistry.NotifyNPCKilled(1234);
Check(faultRegistry.CallbackFaults == 1 && independent.IsCompleted, "callback fault isolation");
Throws<ArgumentException>(() => faultRegistry.Register("bad/uid", new ExampleBossKilled()), "invalid UID");


string tempDirectory = Path.Combine(Path.GetTempPath(),
    "bunny-achievement-verify-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(tempDirectory);
try
{
    string diag = Path.Combine(tempDirectory, "achievements-diagnostics.txt");
    File.WriteAllText(diag, "BUNNY_TEST");
    int verified = BunnyLoader.SourceCompiler.CompiledAssemblyProbe.Verify(
        typeof(ExampleBossKilled).Assembly.Location, diag);
    Check(verified == 2, "compiled assembly probe finds two achievement-only classes");
    string metadataPath = Path.Combine(tempDirectory, "source-achievement-values.txt");
    Check(File.Exists(metadataPath), "compiler exports achievement metadata");
    var achievementRows = File.ReadAllLines(metadataPath);
    Check(achievementRows.Length == 3 && achievementRows[0] == "BACH_SOURCE_V1",
        "achievement metadata sidecar is versioned and complete");
    Check(achievementRows[1].Split('\t').Length == 6 &&
        achievementRows[2].Split('\t').Length == 6,
        "achievement sidecar contains six validated columns");
    Check(File.Exists(Path.Combine(tempDirectory, "source-runtime-values.txt")),
        "existing native defaults wire remains available");
}
finally
{
    Directory.Delete(tempDirectory, true);
}

string fixturePath = typeof(ExampleBossKilled).Assembly.Location;
string Bind(string uid, int operation, int type = 0, int count = 1, string payload = "")
    => BunnyAchievementRuntime.Dispatch(uid, fixturePath, operation, type, count, payload);
string loaded = Bind("transport", 0);
Check(loaded.StartsWith("BACH_META_V1\n", StringComparison.Ordinal) &&
    loaded.Split('\n').Length == 4, "Mono dispatcher discovers both classes from fixture DLL");
Check(Bind("transport", 0) == loaded, "repeated bind does not recreate achievements");
Check(Bind("transport", 1, 10, 1, "0") == "OK|0", "remote NPC event routed without credit");
Check(Bind("transport", 1, 10, 1, "1").StartsWith("BACH_META_V1\n"), "local NPC progress publishes native metadata");
Check(Bind("transport", 1, 1234, 1, "1").StartsWith("BACH_META_V1\n"), "local boss completion publishes native metadata");
string metadata = Bind("transport", 8);
Check(metadata.Contains("|0|0|1|") && metadata.Contains("|1|0|0|"),
    "native-ready metadata includes category hidden completion and progress");
string transportSave = Bind("transport", 5);
Check(transportSave.StartsWith("BACH1\n") && transportSave != "BACH1\n",
    "Mono dispatcher serializes progress");
Check(Bind("transport", 6, 0, 1, "BACH1\n") == "OK", "managed restore clears progress");
Check(Bind("transport", 6, 0, 1, transportSave) == "OK", "managed restore accepts snapshot");
Check(Bind("transport", 8).Contains("|0|0|1|"), "restored completed state visible in UI manifest");
Check(Bind("transport", 1, -32769, 1, "1").StartsWith("ERROR|"),
    "invalid native event rejected");
Check(Bind("transport", 7) == transportSave, "unload returns the last saved snapshot");
Check(Bind("transport", 8).StartsWith("ERROR|"), "unloaded mod cannot receive callbacks");
Check(Bind("transport", 0).StartsWith("BACH_META_V1\n"), "mod can bind after unload");
Check(Bind("transport", 7).StartsWith("BACH1\n"), "second unload succeeds");

string saveRoot = Path.Combine(Path.GetTempPath(),
    "bunny-achievement-storage-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(saveRoot);
try
{
    Check(Bind("diskcase", 0).StartsWith("BACH_META_V1\n"), "disk storage fixture bound");
    Check(Bind("diskcase", 9, payload: saveRoot) == "READY", "configure native data directory");
    Check(!File.Exists(Path.Combine(saveRoot, "diskcase.bach")),
        "configuration does not create unnecessary save");
    Check(Bind("diskcase", 1, 10, 1, "1").StartsWith("BACH_META_V1\n"), "partial progress publishes metadata");
    Check(Bind("diskcase", 10) == "OK", "explicit flush succeeds");
    string diskPath = Path.Combine(saveRoot, "diskcase.bach");
    Check(File.Exists(diskPath) && File.ReadAllText(diskPath).Contains("BACH1\n"),
        "atomic flush produces versioned save file");
    Check(Bind("diskcase", 1, 1234, 1, "1").StartsWith("BACH_META_V1\n"),
        "completion fires immediate autosave");
    string diskData = File.ReadAllText(diskPath);
    Check(diskData.Contains("BACH1\n") && diskData.Length > 20, "completion persisted");
    Check(Bind("diskcase", 7) == diskData, "unload flush preserves final disk state");

    Check(Bind("diskcase", 0).StartsWith("BACH_META_V1\n"), "fresh assembly binding");
    Check(Bind("diskcase", 9, payload: saveRoot) == "READY", "restore prior game session");
    Check(Bind("diskcase", 8).Contains("|0|0|1|"),
        "restored completion visible without replaying unlock");
    Check(Bind("diskcase", 9, payload: saveRoot).StartsWith("ERROR|"),
        "store cannot configure twice");
    Check(Bind("diskcase", 7).StartsWith("BACH1\n"), "restored game session unload");

    string corruptPath = Path.Combine(saveRoot, "broken.bach");
    File.WriteAllText(corruptPath, "BACH1\nBAD|BAD|NaN\n");
    Check(Bind("broken", 0).StartsWith("BACH_META_V1\n"), "corruption fixture bound");
    Check(Bind("broken", 9, payload: saveRoot).StartsWith("ERROR|"),
        "corrupt on-disk save rejected before activation");
    Check(File.ReadAllText(corruptPath).Contains("NaN"),
        "corrupted save not silently overwritten");
    Check(Bind("broken", 7).StartsWith("BACH1\n"), "unconfigured mod clean unload");
}
finally
{
    Directory.Delete(saveRoot, true);
}

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

sealed class NegativeNpcAchievement : BunnyAchievement
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Achievements/Variant.png";
        AddNPCKilledCondition(-4);
    }
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
