using System.Reflection;
using BunnyLoader.Source;

public sealed class ManagedProbeNpc : BunnyNPC
{
    public static int Spawns, Despawns, Hits, Kills, Calls;
    public static readonly List<string> Owners = new();
    public override void SetDefaults() { Owners.Add(ModId); Width=47; }

    public override void OnSpawn(ref BunnyNpcAiState state) => Spawns++;
    public override bool AI(ref BunnyNpcAiState state)
    {
        Calls++;
        state.VelocityX += 1f;
        return true;
    }
    public override void OnHit(ref BunnyNpcAiState state, int damage) => Hits += damage;
    public override void OnKill(ref BunnyNpcAiState state) => Kills++;
    public override void OnDespawn(ref BunnyNpcAiState state) => Despawns++;
}

public sealed class ManagedProbeProjectile : BunnyProjectile
{
    public static int Spawns, Despawns, Hits, Calls, Kills, SeenTarget = -1;
    public static float LastHitX, LastHitY, LastKillX, LastKillY;
    public static readonly List<string> Owners = new();
    public override void SetDefaults() { Owners.Add(ModId); Width=13; }

    public override void OnSpawn(ref BunnyProjectileAiState state) => Spawns++;
    public override bool AI(ref BunnyProjectileAiState state)
    {
        Calls++;
        SeenTarget = state.HasTarget ? state.TargetNpc : -1;
        state.VelocityY = state.Age + 1;
        return true;
    }
    public override void OnHitNPC(ref BunnyProjectileAiState state, int npcIndex, int damage)
    {
        Hits += npcIndex + damage;
        LastHitX = state.X;
        LastHitY = state.Y;
    }
    public override void OnKill(ref BunnyProjectileAiState state)
    {
        Kills++;
        LastKillX = state.X;
        LastKillY = state.Y;
    }
    public override void OnDespawn(ref BunnyProjectileAiState state) => Despawns++;
}

public sealed class BrokenManagedNpc : BunnyNPC
{
    public static int Calls;
    public static bool ShouldThrow = true;
    public override void SetDefaults() { }
    public override bool AI(ref BunnyNpcAiState state)
    {
        Calls++;
        if (ShouldThrow) throw new InvalidOperationException("expected AI failure");
        state.VelocityX = 2;
        return true;
    }
}

public sealed class ManagedProbeDust : BunnyDust
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Effects/TrailblazerSpark.png";
    }
}

public static class Program
{
    private static int checks;
    private static void Check(bool ok, string label)
    {
        if (!ok) throw new InvalidOperationException(label);
        checks++;
    }

    private static float DecodeX(long result) => (short)(result & 0xffff) / 256f;
    private static float DecodeY(long result) => (short)((result >> 16) & 0xffff) / 256f;

    public static void Main()
    {
        string dll = Assembly.GetExecutingAssembly().Location;
        int first = BunnyEntitySourceDispatcher.Register("source-mod-a", dll);
        int second = BunnyEntitySourceDispatcher.Register("source-mod-b", dll);
        Check(first == 0 && second == 1, "mod registration");
        Check(ManagedProbeNpc.Owners.SequenceEqual(new[]{"source-mod-a","source-mod-b"}) &&
            ManagedProbeProjectile.Owners.SequenceEqual(new[]{"source-mod-a","source-mod-b"}),
            "NPC/projectile defaults must receive each real package UID before callbacks");
        Check(BunnyEntitySourceDispatcher.Register("source-mod-a", dll) == first, "registration idempotent");
        var dustResolver = typeof(BunnyDustSourceDispatcher).GetMethod(
            "ModForAssembly", BindingFlags.NonPublic | BindingFlags.Static);
        Check(dustResolver?.Invoke(null, new object[] { Assembly.GetExecutingAssembly() }) as string
              == "source-mod-a", "BunnyDust assembly registered before item use");
        Check(BunnyDustSourceDispatcher.Register("source-mod-a", dll) == 0,
            "Dust registration remains idempotent");
        Check(BunnyDustSourceDispatcher.Register("source-mod-b", dll) == 1,
            "Second mod Dust registration remains idempotent");
        Check(BunnyEntitySourceDispatcher.Bind(first, 0, 697, nameof(ManagedProbeNpc)), "npc bind");
        Check(BunnyEntitySourceDispatcher.Bind(first, 1, 1136, nameof(ManagedProbeProjectile)), "projectile bind");
        Check(BunnyEntitySourceDispatcher.Bind(second, 0, 698, nameof(ManagedProbeNpc)), "second mod bind");
        Check(!BunnyEntitySourceDispatcher.Bind(first, 0, 700, "does-not-exist"), "unknown class");
        long npc1 = BunnyEntitySourceDispatcher.TickNpc(first, 697, 3, 1, 25, 25,
            100, 100, 0, 0, 130, 100);
        long npc2 = BunnyEntitySourceDispatcher.TickNpc(first, 697, 3, 1, 25, 25,
            100, 100, 1, 0, 130, 100);
        Check(DecodeX(npc1) == 1 && DecodeX(npc2) == 2 && ManagedProbeNpc.Spawns == 1, "npc AI and spawn");
        Check(BunnyEntitySourceDispatcher.HitNpc(first, 697, 3, 1, 9, false), "npc onhit");
        Check(!BunnyEntitySourceDispatcher.HitNpc(first, 697, 3, 0, 9, true), "old NPC generation hit blocked");
        Check(BunnyEntitySourceDispatcher.HitNpc(first, 697, 3, 1, 4, true), "NPC kill");
        Check(ManagedProbeNpc.Hits == 13 && ManagedProbeNpc.Kills == 1, "NPC hit and kill counters");
        Check(!BunnyEntitySourceDispatcher.HitNpc(first, 697, 3, 1, 4, true), "no duplicate NPC kill");
        Check(BunnyEntitySourceDispatcher.TickNpc(first, 697, 3, 2, 25, 25,
            100, 100, 0, 0, 130, 100) >= 0 && ManagedProbeNpc.Spawns == 2, "new generation spawn");
        Check(ManagedProbeNpc.Despawns == 1, "NPC generation despawn");
        Check(BunnyEntitySourceDispatcher.KillNpc(first, 697, 3, 2),
            "NPC native kill event");
        Check(!BunnyEntitySourceDispatcher.KillNpc(first, 697, 3, 2),
            "NPC OnKill once per generation");
        Check(ManagedProbeNpc.Kills == 2, "NPC kill counter");
        Check(BunnyEntitySourceDispatcher.TickNpc(first, 697, 3, 3, 25, 25,
            100, 100, 0, 0, 130, 100) >= 0, "NPC recycled after native kill");

        long projectile = BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 12, 0,
            100, 100, 4, 0, 120, 120);
        Check(DecodeX(projectile) == 4 && DecodeY(projectile) == 1, "projectile typed velocity");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 12, 0,
            100, 100, 4, 0, 120, 120) >= 0 && ManagedProbeProjectile.Spawns == 1, "same projectile");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 12, 0,
            100, 100, 4, 0, 120, 120, 7, 135, 155) >= 0 &&
            ManagedProbeProjectile.SeenTarget == 7, "native target identity carried to MonoVM");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 12, 0,
            100, 100, 4, 0, 120, 120, -1, 0, 0) >= 0 &&
            ManagedProbeProjectile.SeenTarget == -1, "native target cleared");
        var steering = new BunnyProjectileAiState {
            X = 100, Y = 100, PlayerX = 90, PlayerY = 90,
            TargetNpc = 7, TargetX = 200, TargetY = 100
        };
        Check(BunnyMinionSteering.FollowOrAttack(ref steering) &&
            steering.VelocityX > 0 && Math.Abs(steering.VelocityY) < 0.01f,
            "native minion chasing steering");
        Check(BunnyEntitySourceDispatcher.HitProjectile(first, 1136, 8, 12, 0, 5, 7), "projectile hit");
        Check(!BunnyEntitySourceDispatcher.HitProjectile(first, 1136, 8, 12, 1, 5, 7), "projectile owner isolation");
        Check(!BunnyEntitySourceDispatcher.HitProjectile(first, 1136, 8, 11, 0, 5, 7), "projectile generation isolation");
        Check(ManagedProbeProjectile.Hits == 12, "projectile hit payload");
        Check(ManagedProbeProjectile.LastHitX == 100 &&
              ManagedProbeProjectile.LastHitY == 100, "projectile sound position retained on hit");
        Check(BunnyEntitySourceDispatcher.KillProjectile(first, 1136, 8, 12, 0),
            "projectile native kill event");
        Check(!BunnyEntitySourceDispatcher.KillProjectile(first, 1136, 8, 12, 0),
            "projectile OnKill once per generation");
        Check(ManagedProbeProjectile.Kills == 1, "projectile kill count");
        Check(ManagedProbeProjectile.LastKillX == 100 &&
              ManagedProbeProjectile.LastKillY == 100, "projectile position retained on kill");
        Check(!BunnyEntitySourceDispatcher.HitProjectile(first, 1136, 8, 12, 0, 5, 7),
            "projectile hit after kill quarantined");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 12, 1,
            100, 100, 4, 0, 120, 120) >= 0 &&
            ManagedProbeProjectile.Spawns == 2 && ManagedProbeProjectile.Despawns == 1,
            "projectile owner slot reused");
        Check(BunnyEntitySourceDispatcher.KillProjectile(first, 1136, 8, 12, 1) &&
            ManagedProbeProjectile.Kills == 2,
            "projectile new owner kill");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 13, 1,
            100, 100, 4, 0, 120, 120) >= 0, "projectile next generation reactivates");

        Check(BunnyEntitySourceDispatcher.Bind(first, 0, 700, nameof(BrokenManagedNpc)), "faulty npc bind");
        Check(BunnyEntitySourceDispatcher.TickNpc(first, 700, 9, 2, 25, 25,
            100, 100, 0, 0, 130, 100) < 0 && BrokenManagedNpc.Calls == 1, "npc failure");
        Check(BunnyEntitySourceDispatcher.TickNpc(first, 700, 9, 2, 25, 25,
            100, 100, 0, 0, 130, 100) < 0 && BrokenManagedNpc.Calls == 1, "quarantine");
        Check(BunnyEntitySourceDispatcher.Status(first).Contains("faults=1"), "failure diagnostic");
        Check(BunnyEntitySourceDispatcher.Status(first).Contains("npcKills=2") &&
              BunnyEntitySourceDispatcher.Status(first).Contains("projectileKills=2"),
              "combat counters exposed");
        BrokenManagedNpc.ShouldThrow = false;
        Check(DecodeX(BunnyEntitySourceDispatcher.TickNpc(first, 700, 9, 3, 25, 25,
            100, 100, 0, 0, 130, 100)) == 2, "quarantine resets on generation");

        for (int i = 0; i < 256; i++)
            BunnyEntitySourceDispatcher.TickNpc(first, 697, 33, 1, 25, 25, 0, 0, 0, 0, 10, 10);
        long before = GC.GetAllocatedBytesForCurrentThread();
        for (int i = 0; i < 10000; i++)
            BunnyEntitySourceDispatcher.TickNpc(first, 697, 33, 1, 25, 25, 0, 0, 0, 0, 10, 10);
        long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
        Check(allocated <= 512, "npc 10k tick allocations " + allocated);
        BunnyEntitySourceDispatcher.ResetWorld();
        Check(ManagedProbeProjectile.Despawns >= 3 && ManagedProbeNpc.Despawns >= 3, "world release");
        Check(BunnyEntitySourceDispatcher.TickProjectile(first, 1136, 8, 13, 0,
            100, 100, 4, 0, 120, 120) >= 0, "next world projectile");
        Console.WriteLine("BUNNY_CSHARP_MONO_ENTITY_SOURCE=PASS checks=" + checks);
        Console.WriteLine("BUNNY_CSHARP_MONO_ENTITY_10000_TICKS=PASS allocated=" + allocated);
    }
}
