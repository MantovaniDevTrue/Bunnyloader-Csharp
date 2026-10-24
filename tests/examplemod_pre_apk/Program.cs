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
Console.WriteLine("EXAMPLEMOD_PRE_APK=PASS random=200 collision_examples=3 (host only)");
