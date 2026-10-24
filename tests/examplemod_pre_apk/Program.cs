using BunnyLoader.Source;
using ExampleModCSharp;
static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
var random = new ExampleRandomCompatProbePlayer();
for (int i = 0; i < 200; i++)
    Check(random.OnEnterWorld() == "Bunny C# Random Compat: PASS (seed/weights/bool/float/vector/list)", "Random probe");
BunnyProjectile[] projectiles = { new ExampleCollisionBounce(), new ExampleCollisionStop(), new ExampleCollisionKeepAlive() };
BunnyWeapon[] wands = { new ExampleCollisionBounceWand(), new ExampleCollisionStopWand(), new ExampleCollisionKeepAliveWand() };
BunnyProjectileTileCollisionMode[] modes = { BunnyProjectileTileCollisionMode.Bounce, BunnyProjectileTileCollisionMode.Stop, BunnyProjectileTileCollisionMode.KeepAlive };
for (int i = 0; i < projectiles.Length; i++) {
    var projectile = projectiles[i]; projectile.SetDefaults(); wands[i].SetDefaults();
    Check(projectile.TileCollisionMode == modes[i] && projectile.TileCollisionSurvivalLimit == 3, "Native collision mode and survival limit");
    Check(projectile.TilePlatformCollision == BunnyProjectilePlatformCollision.SolidPlatforms && projectile.TileCollisionBoxWidth == 6, "Platforms and collision geometry");
    Check(wands[i].Shoot == projectile.GetType().Name, "Wand targets actual projectile");
    Check(File.Exists("samples/ExampleModCSharp/Playable/content/" + projectile.Texture) && File.Exists("samples/ExampleModCSharp/Playable/content/" + wands[i].Texture), "Shared textures exist");
    var state = new BunnyProjectileAiState();
    projectile.OnSpawn(ref state); projectile.OnTileCollide(ref state, 8, 0, 0, 0);
}
Check(ExampleCollisionBounce.Impacts == 1 && ExampleCollisionStop.Impacts == 1 && ExampleCollisionKeepAlive.Impacts == 1, "Real callbacks report impact");
var progress = new ExampleWorldProgressSystem();
progress.ClearWorld();
Check(!ExampleWorldProgressSystem.DownedExampleBoss, "New world resets boss progression");
var empty = new BunnyWorldData();
progress.SaveWorldData(empty);
Check(empty.Count == 0, "Undefeated boss does not persist an empty flag");
ExampleWorldProgressSystem.MarkBossDefeated();
Check(ExampleWorldProgressSystem.DownedExampleBoss, "Boss progression marked");
var saved = new BunnyWorldData();
progress.SaveWorldData(saved);
Check(saved.Count == 1 && saved.GetBool("downedExampleBoss"), "Boss progression saves the original world key");
progress.ClearWorld();
Check(!ExampleWorldProgressSystem.DownedExampleBoss, "World transition clears boss state");
progress.LoadWorldData(saved);
Check(ExampleWorldProgressSystem.DownedExampleBoss, "Boss progression restored from world save");
var network = new BunnyNetWriter();
progress.NetSend(network);
byte[] snapshot = network.ToArray();
Check(snapshot.Length == 2 && snapshot[0] == 1 && snapshot[1] == 1, "Boss network state uses one packed boolean");
progress.ClearWorld();
progress.NetReceive(new BunnyNetReader(snapshot));
Check(ExampleWorldProgressSystem.DownedExampleBoss, "Client state restored from NetReceive");
var falseWriter = new BunnyNetWriter();
progress.ClearWorld();
progress.NetSend(falseWriter);
ExampleWorldProgressSystem.MarkBossDefeated();
progress.NetReceive(new BunnyNetReader(falseWriter.ToArray()));
Check(!ExampleWorldProgressSystem.DownedExampleBoss, "Network state can clear defeated boss flag");
bool malformedRejected = false;
try { progress.NetReceive(new BunnyNetReader(new byte[] { 1 })); }
catch (FormatException) { malformedRejected = true; }
Check(malformedRejected, "Truncated boss flags rejected");
malformedRejected = false;
try { progress.NetReceive(new BunnyNetReader(new byte[] { 1, 1, 7 })); }
catch (FormatException) { malformedRejected = true; }
Check(malformedRejected, "Trailing boss state rejected");
progress.LoadWorldData(saved);
progress.OnWorldExit();
Check(!ExampleWorldProgressSystem.DownedExampleBoss, "World exit clears stale progression");
Console.WriteLine("EXAMPLEMOD_PRE_APK=PASS random=200 collision_examples=3 world_progression=5 (host only)");
