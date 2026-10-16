using BunnyLoader.CSharp;

static class Test
{
    private static int passed;

    private static void Check(bool condition, string name)
    {
        if (!condition) throw new InvalidOperationException(name);
        passed++;
    }

    private sealed class NpcBehavior : BunnyNpcBehavior
    {
        public int Spawned, Despawned, Hit, Killed, Attacks;
        public string FailPhase = "";

        public override void OnSpawn(ref NpcContext context, ref NpcMemory memory)
        {
            Spawned++;
            memory.A = 11;
            if (FailPhase == "spawn")
            {
                context.SetVelocity(99, 99);
                throw new InvalidOperationException("npc spawn");
            }
        }

        public override bool AI(ref NpcContext context, ref NpcMemory memory)
        {
            memory.A++;
            context.SetVelocity(memory.A, 2);
            if (FailPhase == "ai") throw new InvalidOperationException("npc ai");
            return true;
        }

        public override bool TryAttack(ref NpcContext context, ref NpcMemory memory, out NpcCombatCommand attack)
        {
            Attacks++;
            memory.Phase++;
            attack = new NpcCombatCommand { Count = 1, Damage = 8, X = context.X, Y = context.Y };
            if (FailPhase == "attack") throw new InvalidOperationException("npc attack");
            return true;
        }

        public override void OnHit(ref NpcMemory memory, int damage)
        {
            Hit++;
            memory.B += damage;
            if (FailPhase == "hit") throw new InvalidOperationException("npc hit");
        }

        public override void OnKill(ref NpcMemory memory)
        {
            Killed++;
            if (FailPhase == "kill") throw new InvalidOperationException("npc kill");
        }

        public override void OnDespawn(ref NpcMemory memory)
        {
            Despawned++;
            if (FailPhase == "despawn") throw new InvalidOperationException("npc despawn");
        }
    }

    private sealed class ProjectileBehavior : BunnyProjectileBehavior
    {
        public int Spawned, Hit, Killed;
        public string FailPhase = "";

        public override void OnSpawn(ref ProjectileContext context, ref ProjectileMemory memory)
        {
            Spawned++;
            memory.A = 7;
            if (FailPhase == "spawn")
            {
                context.Projectile.SetVelocity(99, 99);
                throw new InvalidOperationException("projectile spawn");
            }
        }

        public override bool AI(ref ProjectileContext context, ref ProjectileMemory memory)
        {
            memory.Counter++;
            context.Projectile.SetVelocity(memory.Counter, 2);
            context.Projectile.SetRotation(0.5f);
            context.Projectile.SetTargetIndex(2);
            if (FailPhase == "ai") throw new InvalidOperationException("projectile ai");
            return true;
        }

        public override void OnHitNPC(ref ProjectileMemory memory, int npcIndex, int damage)
        {
            Hit++;
            memory.A += damage + npcIndex;
            if (FailPhase == "hit") throw new InvalidOperationException("projectile hit");
        }

        public override void OnKill(ref ProjectileMemory memory)
        {
            Killed++;
            if (FailPhase == "kill") throw new InvalidOperationException("projectile kill");
        }
    }

    private static unsafe void CheckNpcs()
    {
        var a = new BunnyNpcRegistry();
        var b = new BunnyNpcRegistry();
        var behavior = new NpcBehavior();
        var separate = new NpcBehavior();
        a.Register("slime", behavior);
        b.Register("slime", separate);
        Check(a.Bind(700, "slime") && b.Bind(700, "slime"), "npc binding");

        NpcState state = new() { Slot = 3, Type = 700, Generation = 5, Life = 25, LifeMax = 25, X = 100, Y = 60 };
        var context = new NpcContext(&state, 160, 60);
        Check(a.Tick(ref context) && a.Tick(ref context), "npc movement");
        Check(behavior.Spawned == 1 && a.Spawns == 1 && a.AiCalls == 2, "npc single spawn");
        Check(state.VelocityX == 13 && state.VelocityY == 2, "npc state");
        Check(a.TryAttack(ref context, out var attack) && attack.Count == 1 && attack.Damage == 8, "npc attack");
        Check(a.NotifyHit(700, 3, 5, 7) && behavior.Hit == 1, "npc hit");
        Check(!a.NotifyHit(700, 3, 4, 7), "npc stale generation hit");
        a.Release(700, 3, 4);
        Check(a.ActiveInstances == 1 && behavior.Despawned == 0, "npc stale release");

        state.Generation = 6;
        Check(a.Tick(ref context) && a.Spawns == 2 && behavior.Despawned == 1, "npc recycled slot");
        Check(!a.NotifyKill(700, 3, 5), "npc stale kill");
        Check(a.NotifyKill(700, 3, 6) && behavior.Killed == 1 && a.ActiveInstances == 0, "npc kill exactly once");
        Check(!a.NotifyKill(700, 3, 6), "npc duplicate kill");

        Check(b.Tick(ref context) && separate.Spawned == 1 && a.ActiveInstances == 0, "npc separate mod");
        b.ResetWorld();
        Check(b.ActiveInstances == 0 && separate.Despawned == 1, "npc world cleanup");

        var failing = new NpcBehavior { FailPhase = "ai" };
        var broken = new BunnyNpcRegistry();
        broken.Register("broken", failing);
        Check(broken.Bind(701, "broken"), "npc fault bind");
        state.Type = 701;
        state.Generation = 7;
        state.VelocityX = 3;
        state.VelocityY = -4;
        Check(!broken.Tick(ref context) && broken.Faults == 1, "npc AI quarantine");
        Check(state.VelocityX == 3 && state.VelocityY == -4, "npc velocity rollback");
        Check(!broken.Tick(ref context) && broken.Faults == 1, "npc failure not repeated");
        Check(!broken.NotifyHit(701, 3, 7, 3), "npc blocked hit");
        broken.Release(701, 3, 7);
        failing.FailPhase = "despawn";
        state.Generation = 8;
        Check(broken.Tick(ref context), "npc next generation recovers");
        broken.ResetWorld();
        Check(broken.ActiveInstances == 0 && broken.Faults == 2, "npc faulty despawn cleanup");
        Check(broken.LastError.Contains("OnDespawn"), "npc diagnostic");

        var spawned = new NpcBehavior { FailPhase = "spawn" };
        var spawnRegistry = new BunnyNpcRegistry();
        spawnRegistry.Register("spawn", spawned);
        Check(spawnRegistry.Bind(702, "spawn"), "npc spawn bind");
        state.Type = 702;
        state.Generation = 9;
        state.VelocityX = 1;
        state.VelocityY = 0;
        Check(!spawnRegistry.Tick(ref context) && spawnRegistry.Faults == 1 && state.VelocityX == 1, "npc spawn failure rollback");
        Check(!spawnRegistry.Tick(ref context) && spawned.Spawned == 1, "npc spawn quarantine");

        var warm = new BunnyNpcRegistry();
        warm.Register("warm", new NpcBehavior());
        Check(warm.Bind(703, "warm"), "npc hot path bind");
        state.Type = 703;
        state.Generation = 10;
        for (int i = 0; i < 256; i++) warm.Tick(ref context);
        long before = GC.GetAllocatedBytesForCurrentThread();
        for (int i = 0; i < 10000; i++) warm.Tick(ref context);
        long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
        Check(allocated <= 512 && warm.AiCalls == 10256 && warm.Spawns == 1, "npc allocation-free 10k ticks " + allocated);
    }

    private static unsafe void CheckProjectiles()
    {
        var behavior = new ProjectileBehavior();
        var r = new BunnyProjectileRegistry();
        r.Register("bolt", behavior);
        Check(r.Bind(1136, "bolt"), "projectile binding");
        ProjectileState state = new() { Slot = 50, Type = 1136, Owner = 0, Generation = 4, Vx = 7, Vy = 0 };
        NpcSample* npcs = stackalloc NpcSample[1];
        npcs[0] = new NpcSample { Index = 5, Type = 1, Life = 10, LifeMax = 10, Flags = 1 };
        var context = new ProjectileContext(&state, npcs, 1, 300, 150);
        Check(r.Tick(ref context) && r.Tick(ref context), "projectile movement");
        Check(r.SpawnCount == 1 && behavior.Spawned == 1 && r.AiCalls == 2, "projectile single spawn");
        Check(state.Vx == 2 && state.Rotation == 0.5f && state.TargetIndex == 2, "projectile mutable state");
        Check(r.NotifyHitNPC(1136, 50, 0, 4, 5, 10) && behavior.Hit == 1, "projectile hit");
        Check(!r.NotifyHitNPC(1136, 50, 1, 4, 5, 10), "projectile owner isolation");
        Check(!r.NotifyHitNPC(1136, 50, 0, 4, -1, 10), "projectile invalid hit");

        state.Generation = 5;
        Check(r.Tick(ref context) && r.SpawnCount == 2 && behavior.Killed == 1, "projectile recycled slot");
        r.Release(1136, 50, 0, 4);
        Check(r.ActiveInstances == 1, "projectile stale release");
        r.Release(1136, 50, 0, 5);
        Check(r.ActiveInstances == 0 && behavior.Killed == 2, "projectile final kill");
        r.Release(1136, 50, 0, 5);
        Check(behavior.Killed == 2, "projectile duplicate release");

        var failure = new ProjectileBehavior { FailPhase = "ai" };
        var broken = new BunnyProjectileRegistry();
        broken.Register("bad", failure);
        Check(broken.Bind(1137, "bad"), "projectile fault bind");
        state.Type = 1137;
        state.Generation = 6;
        state.Vx = 7;
        state.Vy = 3;
        state.Rotation = 1;
        state.TargetIndex = -1;
        Check(!broken.Tick(ref context) && broken.Faults == 1, "projectile AI quarantine");
        Check(state.Vx == 7 && state.Vy == 3 && state.Rotation == 1 && state.TargetIndex == -1, "projectile rollback");
        Check(!broken.Tick(ref context) && broken.Faults == 1 && !broken.NotifyHitNPC(1137, 50, 0, 6, 1, 5), "projectile blocked events");
        broken.Release(1137, 50, 0, 6);
        failure.FailPhase = "kill";
        state.Generation = 7;
        Check(broken.Tick(ref context), "projectile new generation recovery");
        broken.ResetWorld();
        Check(broken.ActiveInstances == 0 && broken.Faults == 2 && broken.LastError.Contains("OnKill"), "projectile kill exception isolation");

        var hitFailure = new ProjectileBehavior { FailPhase = "hit" };
        var hitRegistry = new BunnyProjectileRegistry();
        hitRegistry.Register("hit", hitFailure);
        Check(hitRegistry.Bind(1140, "hit"), "projectile hit bind");
        state.Type = 1140;
        state.Generation = 8;
        Check(hitRegistry.Tick(ref context), "projectile hit setup");
        Check(!hitRegistry.NotifyHitNPC(1140, 50, 0, 8, 0, 15) && hitRegistry.Faults == 1, "projectile hit quarantine");
        Check(!hitRegistry.NotifyHitNPC(1140, 50, 0, 8, 0, 15) && hitFailure.Hit == 1, "projectile hit failure not repeated");

        var warm = new BunnyProjectileRegistry();
        warm.Register("warm", new ProjectileBehavior());
        Check(warm.Bind(1141, "warm"), "projectile hot path bind");
        state.Type = 1141;
        state.Generation = 9;
        for (int i = 0; i < 256; i++) warm.Tick(ref context);
        long before = GC.GetAllocatedBytesForCurrentThread();
        for (int i = 0; i < 10000; i++) warm.Tick(ref context);
        long allocated = GC.GetAllocatedBytesForCurrentThread() - before;
        Check(allocated <= 512 && warm.AiCalls == 10256 && warm.SpawnCount == 1, "projectile allocation-free 10k ticks " + allocated);
    }

    public static unsafe void Main()
    {
        CheckNpcs();
        CheckProjectiles();
        Console.WriteLine("BUNNY_CSHARP_ENTITY_LIFECYCLE=PASS checks=" + passed);
        Console.WriteLine("BUNNY_CSHARP_ENTITY_STRESS=PASS npcTicks=10000 projectileTicks=10000");
    }
}
