using System;
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
    public static void Main()
    {
        int mod = BunnyEntitySourceDispatcher.Register("csharpentitycontractprobe", typeof(Program).Assembly.Location);
        Check.True(mod >= 0, "Register failed: " + BunnyEntitySourceDispatcher.LastRegistrationError);
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 0, 42, "MissingNpc"), "Missing NPC incorrectly bound");
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 1, 65, "MissingProjectile"), "Missing projectile incorrectly bound");
        Check.True(!BunnyEntitySourceDispatcher.Bind(mod, 2, 44, "TestNpc"), "Unknown kind accepted");
        Check.True(BunnyEntitySourceDispatcher.Bind(mod, 0, 42, "TestNpc"), "NPC binding rejected");
        Check.True(BunnyEntitySourceDispatcher.Bind(mod, 1, 65, "TestProjectile"), "Projectile binding rejected");

        long npc = BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25,
            0f, 0f, 0f, 0f, 8f, 8f);
        Check.True(npc != -1, "NPC AI state change not forwarded");
        BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25, 0f, 0f, 0f, 0f, 8f, 8f);
        Check.Equal(TestNpc.Spawns, 1, "NPC spawned twice for same generation");
        Check.Equal(TestNpc.Ai, 2, "NPC AI dispatch mismatch");
        Check.True(BunnyEntitySourceDispatcher.HitNpc(mod, 42, 5, 1001, 5, false), "NPC hit not delivered");
        Check.True(BunnyEntitySourceDispatcher.KillNpc(mod, 42, 5, 1001), "NPC kill not delivered");
        Check.True(!BunnyEntitySourceDispatcher.KillNpc(mod, 42, 5, 1001), "NPC kill delivered twice");
        Check.Equal(TestNpc.Hits, 1, "NPC hit callbacks incorrect");
        Check.Equal(TestNpc.Kills, 1, "NPC kill callbacks incorrect");

        long projectile = BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2,
            0f, 0f, 0f, 0f, 8f, 8f, -1, 0f, 0f);
        Check.True(projectile != -1, "Projectile AI state change not forwarded");
        BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2, 0f, 0f, 0f, 0f, 8f, 8f);
        Check.Equal(TestProjectile.Spawns, 1, "Projectile spawned twice for same generation");
        Check.Equal(TestProjectile.Ai, 2, "Projectile AI dispatch mismatch");
        Check.True(BunnyEntitySourceDispatcher.CollideProjectile(mod, 65, 8, 2001, 2, 8f, 0f, 0f, 0f), "Projectile collision not delivered");
        Check.Equal(TestProjectile.TileCollisions, 1, "Projectile collision fired an incorrect number of times");
        Check.True(TestProjectile.LastOldX == 8f && TestProjectile.LastNewX == 0f, "Projectile collision velocities changed");
        TestProjectile.RequestBounce = true;
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2001, 2,
            8f, 0f, 0f, 0f), 3, "Bounce request was not encoded for later native impact");
        TestProjectile.RequestBounce = false;
        TestProjectile.RequestVanilla = true;
        Check.Equal(BunnyEntitySourceDispatcher.CollideProjectilePolicy(mod, 65, 8, 2001, 2,
            8f, 0f, 0f, 0f), 0, "Unsupported same-frame vanilla mode was accepted");
        TestProjectile.RequestVanilla = false;
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
        Check.Equal(TestProjectile.Hits, 1, "Projectile hit callbacks incorrect");
        Check.Equal(TestProjectile.Kills, 1, "Projectile kill callbacks incorrect");

        BunnyEntitySourceDispatcher.ResetWorld();
        Check.Equal(TestNpc.Despawns, 1, "NPC was not released on reset");
        Check.Equal(TestProjectile.Despawns, 1, "Projectile was not released on reset");
        Check.True(BunnyEntitySourceDispatcher.TickNpc(mod, 42, 5, 1001, 25, 25,
            0f, 0f, 0f, 0f, 8f, 8f) != -1, "NPC was not reset for next world");
        Check.True(BunnyEntitySourceDispatcher.TickProjectile(mod, 65, 8, 2001, 2,
            0f, 0f, 0f, 0f, 8f, 8f, -1, 0f, 0f) != -1, "Projectile was not reset for next world");
        Check.Equal(TestNpc.Spawns, 2, "NPC did not respawn after reset");
        Check.Equal(TestProjectile.Spawns, 2, "Projectile did not respawn after reset");
        BunnyEntitySourceDispatcher.ResetWorld();
        Check.Equal(TestNpc.Despawns, 2, "NPC final cleanup incorrect");
        Check.Equal(TestProjectile.Despawns, 2, "Projectile final cleanup incorrect");
        Console.WriteLine("MANAGED_ENTITY_LIFECYCLE_PASS");
    }
}
