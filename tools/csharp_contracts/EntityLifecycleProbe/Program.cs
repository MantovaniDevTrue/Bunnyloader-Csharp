using System;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using BunnyLoader.Source;

static class Check
{
    public static void True(bool actual, string message)
    {
        if (!actual) throw new InvalidOperationException(message);
    }

    public static void Equal(int actual, int expected, string message)
    {
        if (actual != expected)
            throw new InvalidOperationException(message + $" expected={expected} actual={actual}");
    }
}

public sealed class TestNpc : BunnyNPC
{
    public static int Spawns, Ai, Hits, Kills, Despawns;

    public override void SetDefaults() { }

    public override void OnSpawn(ref BunnyNpcAiState state) => Spawns++;

    public override bool AI(ref BunnyNpcAiState state)
    {
        Ai++;
        state.VelocityX = 1.25f;
        return true;
    }

    public override void OnHit(ref BunnyNpcAiState state, int damage) => Hits++;
    public override void OnKill(ref BunnyNpcAiState state) => Kills++;
    public override void OnDespawn(ref BunnyNpcAiState state) => Despawns++;
}

public sealed class TestProjectile : BunnyProjectile
{
    public static int Spawns, Ai, Hits, Kills, Despawns, TileCollisions;
    public static float LastOldX, LastNewX;
    public static bool RequestBounce;
    public static bool RequestVanilla;

    public override void SetDefaults() { }

    public override void OnTileCollide(ref BunnyProjectileAiState state,
        float oldVelocityX, float oldVelocityY, float newVelocityX, float newVelocityY)
    {
        TileCollisions++;
        LastOldX = oldVelocityX;
        LastNewX = newVelocityX;
        if (RequestBounce) state.NextTileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
        if (RequestVanilla) state.NextTileCollisionMode = BunnyProjectileTileCollisionMode.Vanilla;
    }

    public override void OnSpawn(ref BunnyProjectileAiState state) => Spawns++;

    public override bool AI(ref BunnyProjectileAiState state)
    {
        Ai++;
        state.VelocityY = -1f;
        return true;
    }

    public override void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage) => Hits++;
    public override void OnKill(ref BunnyProjectileAiState state) => Kills++;
    public override void OnDespawn(ref BunnyProjectileAiState state) => Despawns++;
}

public static class Program
{
    private static Type LoadedType(string name)
    {
        var context = AssemblyLoadContext.All.Single(c => c.Name == "BunnyMod:csharpentitycontractprobe");
        var assembly = context.Assemblies.Single(a => a.GetName().Name == typeof(Program).Assembly.GetName().Name);
        return assembly.GetType(name, throwOnError: true)!;
    }

    private static int Counter(Type type, string name)
        => (int)type.GetField(name, BindingFlags.Public | BindingFlags.Static)!.GetValue(null)!;

    private static void SetFlag(Type type, string name, bool value)
        => type.GetField(name, BindingFlags.Public | BindingFlags.Static)!.SetValue(null, value);

    public static void Main()
    {
        int mod = BunnyEntitySourceDispatcher.Register("csharpentitycontractprobe", typeof(Program).Assembly.Location);
        Check.True(mod >= 0, "Register failed: " + BunnyEntitySourceDispatcher.LastRegistrationError);
        Type npcType = LoadedType(nameof(TestNpc));
        Type projectileType = LoadedType(nameof(TestProjectile));
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 0, 42, "MissingNpc"), "Missing NPC incorrectly bound");
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 1, 65, "MissingProjectile"), "Missing projectile incorrectly bound");
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 2, 44, "TestNpc"), "Unknown kind accepted");
        Check.True(BunnyEntitySourceDispatcher.Bind(mod, 0, 42, "TestNpc"), "NPC binding rejected");
        Check.True(BunnyEntitySourceDispatcher.Bind(mod, 1, 65, "TestProjectile"), "Projectile binding rejected");

        long npc = BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25,
            0f, 0f, 0f, 0f, 8f, 8f);
        Check.True(npc != -1, "NPC AI state change not forwarded");
        BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25, 0f, 0f, 0f, 0f, 8f, 8f);
        Check.Equal(Counter(npcType, nameof(TestNpc.Spawns)), 1, "NPC spawned twice for same generation");
        Check.Equal(Counter(npcType, nameof(TestNpc.Ai)), 2, "NPC AI dispatch mismatch");
        Check.True(BunnyEntitySourceDispatcher.HitNpc(mod, 42, 5, 1001, 5, false), "NPC hit not delivered");
        Check.True(BunnyEntitySourceDispatcher.KillNpc(mod, 42, 5, 1001), "NPC kill not delivered");
        Check.True(!BunnyEntitySourceDispatcher.KillNpc(mod, 42, 5, 1001), "NPC kill delivered twice");
        Check.Equal(Counter(npcType, nameof(TestNpc.Hits)), 1, "NPC hit callbacks incorrect");
        Check.Equal(Counter(npcType, nameof(TestNpc.Kills)), 1, "NPC kill callbacks incorrect");

        long projectile = BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2,
            0f, 0f, 0f, 0f, 8f, 8f, -1, 0f, 0f);
        Check.True(projectile != -1, "Projectile AI state change not forwarded");
        BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2, 0f, 0f, 0f, 0f, 8f, 8f);
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Spawns)), 1, "Projectile spawned twice for same generation");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Ai)), 2, "Projectile AI dispatch mismatch");
        Check.True(BunnyEntitySourceDispatcher.CollideProjectile(mod, 65, 8, 2001, 2, 8f, 0f, 0f, 0f), "Projectile collision not delivered");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.TileCollisions)), 1, "Projectile collision fired an incorrect number of times");
        Check.True(Convert.ToSingle(projectileType.GetField(nameof(TestProjectile.LastOldX))!.GetValue(null)) == 8f && Convert.ToSingle(projectileType.GetField(nameof(TestProjectile.LastNewX))!.GetValue(null)) == 0f, "Projectile collision velocities changed");
        SetFlag(projectileType, nameof(TestProjectile.RequestBounce), true);
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2001, 2,
            8f, 0f, 0f, 0f), 3, "Bounce request was not encoded for later native impact");
        SetFlag(projectileType, nameof(TestProjectile.RequestBounce), false);
        SetFlag(projectileType, nameof(TestProjectile.RequestVanilla), true);
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2001, 2,
            8f, 0f, 0f, 0f), 0, "Unsupported same-frame vanilla mode was accepted");
        SetFlag(projectileType, nameof(TestProjectile.RequestVanilla), false);
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2001, 2,
            8f, 0f, 0f, 0f), 1, "Default collision callback changed ABI");
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2002, 2,
            8f, 0f, 0f, 0f), 0, "Stale instance returned a policy request");
        Check.True(!BunnyEntitySourceDispatcher.CollideProjectile(mod, 65, 8, 2002, 2, 8f, 0f, 0f, 0f), "Stale collision accepted");
        Check.True(!BunnyEntitySourceDispatcher.CollideProjectile(mod, 65, 8, 2001, 2, float.NaN, 0f, 0f, 0f), "Invalid collision velocity accepted");
        Check.True(BunnyEntitySourceDispatcher.HitProjectile(mod, 65, 8, 2001, 2, 5, 10), "Projectile hit not delivered");
        Check.True(BunnyEntitySourceDispatcher.KillProjectile(mod, 65, 8, 2001, 2), "Projectile kill not delivered");
        Check.True(!BunnyEntitySourceDispatcher.KillProjectile(mod, 65, 8, 2001, 2), "Projectile kill delivered twice");
        Check.True(!BunnyEntitySourceDispatcher.CollideProjectile(mod, 65, 8, 2001, 2, 8f, 0f, 0f, 0f), "Collision accepted after kill");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Hits)), 1, "Projectile hit callbacks incorrect");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Kills)), 1, "Projectile kill callbacks incorrect");

        BunnyEntitySourceDispatcher.ResetWorld();
        Check.Equal(Counter(npcType, nameof(TestNpc.Despawns)), 1, "NPC was not released on reset");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Despawns)), 1, "Projectile was not released on reset");
        Check.True(BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25,
            0f, 0f, 0f, 0f, 8f, 8f) != -1, "NPC was not reset for next world");
        Check.True(BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2,
            0f, 0f, 0f, 0f, 8f, 8f, -1, 0f, 0f) != -1, "Projectile was not reset for next world");
        Check.Equal(Counter(npcType, nameof(TestNpc.Spawns)), 2, "NPC did not respawn after reset");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Spawns)), 2, "Projectile did not respawn after reset");
        BunnyEntitySourceDispatcher.ResetWorld();
        Check.Equal(Counter(npcType, nameof(TestNpc.Despawns)), 2, "NPC final cleanup incorrect");
        Check.Equal(Counter(projectileType, nameof(TestProjectile.Despawns)), 2, "Projectile final cleanup incorrect");
        Console.WriteLine("MANAGED_ENTITY_LIFECYCLE_PASS");
    }
}
