using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

public static class BunnyEntitySourceDispatcher
{
    private struct NpcInstance
    {
        public int Type;
        public int Generation;
        public int Age;
        public bool Blocked;
    }

    private struct ProjectileInstance
    {
        public int Type;
        public int Generation;
        public int Owner;
        public int Age;
        public float X;
        public float Y;
        public float VelocityX;
        public float VelocityY;
        public bool Blocked;
    }

    private sealed class Context
    {
        public string Mod = "";
        public string Path = "";
        public BunnyModAssemblyLease? Lease;
        public readonly Dictionary<string, BunnyNPC> NamedNpcs = new(StringComparer.Ordinal);
        public readonly Dictionary<string, BunnyProjectile> NamedProjectiles = new(StringComparer.Ordinal);
        public readonly Dictionary<int, BunnyNPC> Npcs = new();
        public readonly Dictionary<int, BunnyProjectile> Projectiles = new();
        public readonly Dictionary<string, BunnyItem> ItemUsers = new(StringComparer.Ordinal);
        public readonly HashSet<string> RegisteredItemUses = new(StringComparer.Ordinal);
        public readonly Dictionary<string, BunnyInvasionSystem> Invasions = new(StringComparer.Ordinal);
        public readonly List<BunnyInvasionSystem> InvasionOrder = new();
        public readonly Dictionary<string, BunnyInvasionClientView> ClientViews = new(StringComparer.Ordinal);
        public readonly Dictionary<string, int> NetRevisions = new(StringComparer.Ordinal);
        public readonly Dictionary<string, int> HudRevisions = new(StringComparer.Ordinal);
        public readonly Dictionary<string, (BunnyInvasionPhase Phase, int Progress, int Cooldown)> SavedInvasions = new(StringComparer.Ordinal);
        public readonly Dictionary<string, (BunnyInvasionPhase Phase, int PercentStep)> NotifiedInvasions = new(StringComparer.Ordinal);
        public int NextInvasionIndex;
        public int SaveTimerFrames;
        public readonly HashSet<string> PendingRewards = new(StringComparer.Ordinal);
        public int RewardPlayer = -1;
        public int LastWorldPlayer = -1;
        public long NativeWorldSession;
        public bool WorldStateInitialized;
        public readonly NpcInstance[] NpcSlots = new NpcInstance[512];
        public readonly ProjectileInstance[] ProjectileSlots = new ProjectileInstance[4096];
        public string LastError = "";
        public string LastUse = "None";
        public long NpcCalls, ProjectileCalls, NpcHits, NpcKills, ProjectileHits, ProjectileKills, Faults;
    }

    private static readonly List<Context> Contexts = new(16);
    public static string LastRegistrationError { get; private set; } = "";

    public static int Register(string modId, string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(modId) || string.IsNullOrWhiteSpace(assemblyPath))
            return -1;
        for (int i = 0; i < Contexts.Count; ++i)
        {
            if (Contexts[i].Mod == modId)
                return Contexts[i].Path == assemblyPath ? i : -1;
        }
        if (Contexts.Count >= 16) return -1;
        BunnyModAssemblyLease? lease = null;
        try
        {
            lease = BunnyModAssemblyRegistry.Acquire(modId, assemblyPath);
            var assembly = lease.Assembly;
            var context = new Context { Mod = modId, Path = assemblyPath, Lease = lease };
            bool hasDust = false;
            foreach (Type type in assembly.GetTypes().OrderBy(t => t.FullName, StringComparer.Ordinal))
            {
                if (!type.IsClass || type.IsAbstract || type.ContainsGenericParameters) continue;
                if (typeof(BunnyNPC).IsAssignableFrom(type))
                {
                    if (Activator.CreateInstance(type) is BunnyNPC npc &&
                        !context.NamedNpcs.TryAdd(type.Name, npc))
                        throw new InvalidOperationException("Duplicated NPC class " + type.Name);
                }
                else if (typeof(BunnyProjectile).IsAssignableFrom(type))
                {
                    if (Activator.CreateInstance(type) is BunnyProjectile projectile &&
                        !context.NamedProjectiles.TryAdd(type.Name, projectile))
                        throw new InvalidOperationException("Duplicated projectile class " + type.Name);
                }
                else if (typeof(BunnyItem).IsAssignableFrom(type))
                {
                    MethodInfo? use = type.GetMethod(nameof(BunnyItem.OnUse),
                        new[] { typeof(int) });
                    MethodInfo? contextual = type.GetMethod(nameof(BunnyItem.OnUse),
                        new[] { typeof(int), typeof(BunnyUseContext) });
                    if ((use?.DeclaringType != typeof(BunnyItem) ||
                         contextual?.DeclaringType != typeof(BunnyItem)) &&
                        Activator.CreateInstance(type) is BunnyItem item)
                    {
                        if (!context.ItemUsers.TryAdd(type.Name, item))
                            throw new InvalidOperationException("Duplicate item use class " + type.Name);
                    }
                }
                else if (typeof(BunnyDust).IsAssignableFrom(type))
                {
                    hasDust = true;
                }
                else if (typeof(BunnyInvasionSystem).IsAssignableFrom(type))
                {
                    BunnyInvasionSystem system = Activator.CreateInstance(type) as BunnyInvasionSystem
                        ?? throw new InvalidOperationException("Invalid invasion system " + type.Name);
                    if (context.Invasions.Count >= 16 || !context.Invasions.TryAdd(system.Id, system))
                        throw new InvalidOperationException("Duplicate invasion ID or too many invasion systems: " + system.Id);
                    context.InvasionOrder.Add(system);
                }
            }
            if (hasDust && BunnyDustSourceDispatcher.RegisterLoaded(modId, assembly) < 0)
                context.LastError = "BunnyDust registration: " +
                    BunnyDustSourceDispatcher.LastRegistrationError;
            Contexts.Add(context);
            lease = null;
            return Contexts.Count - 1;
        }
        catch (Exception error)
        {
            LastRegistrationError = error.GetType().Name + ": " + error.Message;
            return -1;
        }
        finally { lease?.Dispose(); }
    }

    internal static bool TryRegisterDustAssembly(Assembly assembly)
    {
        string path = assembly.Location;
        if (path.Length == 0 && BunnyModAssemblyRegistry.TryResolve(assembly, out _, out string resolved))
            path = resolved;
        foreach (Context context in Contexts)
        {
            if (context.Path != path) continue;
            int slot = BunnyDustSourceDispatcher.RegisterLoaded(context.Mod, assembly);
            if (slot >= 0) return true;
            context.LastError = "BunnyDust registration: " +
                BunnyDustSourceDispatcher.LastRegistrationError;
            return false;
        }
        return false;
    }

    public static bool Bind(int modSlot, int kind, int type, string name)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || type < 0 || string.IsNullOrWhiteSpace(name))
            return false;
        Context context = Contexts[modSlot];
        if (kind == 0 && context.NamedNpcs.TryGetValue(name, out BunnyNPC? npc))
        {
            context.Npcs[type] = npc;
            return true;
        }
        if (kind == 1 && context.NamedProjectiles.TryGetValue(name, out BunnyProjectile? projectile))
        {
            context.Projectiles[type] = projectile;
            return true;
        }
        return false;
    }

    public static int TickWorld(int modSlot, int mode, int elapsedFrames,
        long worldSession, int playerIndex, float playerX, float playerY)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || worldSession <= 0 ||
            elapsedFrames < 1 || elapsedFrames > 60) return 0;
        Context context = Contexts[modSlot];
        if (context.Invasions.Count == 0 && context.ItemUsers.Count == 0) return -2;
        bool authoritative = mode == 0 || mode == 2 || mode == 3;
        if (context.NativeWorldSession != worldSession)
        {
            foreach (BunnyInvasionSystem system in context.Invasions.Values) system.OnWorldExit();
            context.RegisteredItemUses.Clear();
            context.SavedInvasions.Clear();
            context.NotifiedInvasions.Clear();
            context.ClientViews.Clear();
            context.NetRevisions.Clear();
            context.HudRevisions.Clear();
            context.WorldStateInitialized = false;
            context.PendingRewards.Clear();
            context.RewardPlayer = -1;
            context.LastWorldPlayer = -1;
            context.LastUse = "None";
            context.NativeWorldSession = worldSession;
            context.NextInvasionIndex = 0;
            context.SaveTimerFrames = 0;
        }
        if (mode == 1)
        {
            foreach (string itemName in context.ItemUsers.Keys)
                BunnyNativeItemUse.Bind(worldSession, context.Mod, itemName);
            try
            {
                int updates = 0;
                while (updates < 8 && BunnyInvasionNativeNetwork.TryPoll(
                    worldSession, context.Mod, out BunnyInvasionWireState state))
                {
                    updates++;
                    if (!context.Invasions.ContainsKey(state.InvasionId)) continue;
                    if (!context.ClientViews.TryGetValue(state.InvasionId, out var view))
                        context.ClientViews[state.InvasionId] = view = new BunnyInvasionClientView();
                    bool hadState = view.HasState;
                    BunnyInvasionWireState previous = view.Current;
                    if (view.ReceiveRemoteFromHost(state, worldSession, context.Mod,
                        state.InvasionId))
                    {
                        BunnyInvasionHud.Publish(worldSession, state);
                        bool significant = !hadState || previous.Phase != state.Phase ||
                            previous.Progress * 10 / previous.Goal !=
                            state.Progress * 10 / state.Goal;
                        if (significant)
                            BunnyInvasionNotifications.Publish(worldSession,
                                context.Mod, state);
                    }
                }
                return updates;
            }
            catch (Exception error)
            {
                context.LastError = "TickWorld(client): " + error.GetType().Name +
                    ": " + error.Message;
                return -1;
            }
        }
        if (!authoritative || playerIndex < 0 ||
            !float.IsFinite(playerX) || !float.IsFinite(playerY)) return 0;
        context.LastWorldPlayer = playerIndex;
        try
        {
            if (!context.WorldStateInitialized)
            {
                foreach (BunnyInvasionSystem system in context.Invasions.Values)
                {
                    BunnyInvasionController controller = system.Pipeline.Controller;
                    BunnyInvasionWorldPersistence.TryRestore(worldSession, context.Mod, controller);
                    BunnyInvasionSnapshot initial = controller.Snapshot();
                    context.SavedInvasions[system.Id] =
                        (initial.Phase, initial.Progress, initial.Cooldown);
                }
                context.WorldStateInitialized = true;
            }
            int used = 0;
            foreach (var pair in context.ItemUsers)
            {
                if (!context.RegisteredItemUses.Contains(pair.Key))
                {
                    BunnyNativeItemUse.Bind(worldSession, context.Mod, pair.Key);
                    if (BunnyNativeItemUse.IsReady(worldSession, context.Mod, pair.Key))
                        context.RegisteredItemUses.Add(pair.Key);
                }
                if (!context.RegisteredItemUses.Contains(pair.Key)) continue;
                for (int n = 0; n < 8 && BunnyNativeItemUse.TryPollContext(
                    worldSession, context.Mod, pair.Key, out int usingPlayer,
                    out BunnyUseContext useContext); n++)
                {
                    context.RewardPlayer = usingPlayer;
                    try
                    {
                        bool applied = pair.Value.OnUse(usingPlayer, useContext);
                        context.LastUse = pair.Key + " Flags=" + useContext.Flags +
                            " Applied=" + (applied ? "1" : "0");
                        if (!applied)
                        {
                            string reason = pair.Value.OnUseRejectionReason(usingPlayer, useContext);
                            if (!string.IsNullOrWhiteSpace(reason))
                                context.LastUse += " Reason=" + reason;
                        }
                    }
                    catch (Exception error)
                    {
                        context.Faults++;
                        context.LastError = "OnUse(" + pair.Key + "): " +
                            error.GetType().Name + ": " + error.Message;
                        context.LastUse = pair.Key + " Flags=" + useContext.Flags +
                            " Applied=0 Error=" + error.GetType().Name +
                            ": " + error.Message;
                    }
                    used++;
                    SaveInvasionIfChanged(context);
                }
            }
            int received = 0;
            while (received < 8 && BunnyInvasionNativeTransport.TryPoll(
                worldSession, context.Mod, out BunnyNativeInvasionReply result))
            {
                received++;
                foreach (BunnyInvasionSystem system in context.Invasions.Values)
                {
                    if (!system.Pipeline.TryGetReservedClass(
                        result.Epoch, result.Ticket, out string reservedClass)) continue;
                    if (result.Success)
                        system.OnNativeCreated(true, new BunnyInvasionSpawn(
                            result.Ticket, reservedClass) { Epoch = result.Epoch },
                            result.Slot, result.Identity);
                    else
                        system.OnNativeFailed(true, result.Epoch, result.Ticket);
                    break;
                }
            }
            foreach (BunnyInvasionSystem system in context.Invasions.Values)
                system.Tick(true, elapsedFrames, true);
            context.SaveTimerFrames += elapsedFrames;
            bool periodicSave = context.SaveTimerFrames >= 600;
            if (periodicSave) context.SaveTimerFrames = 0;
            SaveInvasionIfChanged(context, periodicSave);
            if (context.Invasions.Count == 0) return received + used;
            List<BunnyInvasionSystem> systems = context.InvasionOrder;
            for (int iteration = 0; iteration < systems.Count; iteration++)
            {
                int index = (context.NextInvasionIndex + iteration) % systems.Count;
                BunnyInvasionSystem system = systems[index];
                if (system.Pipeline.PendingCount >= 2 ||
                    !system.Pipeline.Controller.CanSpawn) continue;
                int roll = HashCode.Combine(worldSession, elapsedFrames,
                    system.Id, system.Pipeline.Controller.Snapshot().Revision);
                if (!system.TryReserve(true, roll, out BunnyInvasionSpawn spawn)) continue;
                context.NextInvasionIndex = (index + 1) % systems.Count;
                float x = Math.Clamp(playerX + 320f, 640f, 10000000f);
                float y = Math.Clamp(playerY - 160f, 640f, 10000000f);
                if (!BunnyInvasionNativeTransport.Submit(worldSession, spawn,
                    context.Mod, playerIndex, x, y))
                    system.OnNativeFailed(true, spawn.Epoch, spawn.Ticket);
                return received + 1;
            }
            return received + (used > 0 ? 1 : 0);
        }
        catch (Exception error)
        {
            context.LastError = "TickWorld: " + error.GetType().Name + ": " + error.Message;
            return -1;
        }
    }

    public static bool TryGetInvasionView(int modSlot, string invasionId,
        out BunnyInvasionWireState state)
    {
        state = default;
        if ((uint)modSlot >= (uint)Contexts.Count ||
            string.IsNullOrEmpty(invasionId)) return false;
        return Contexts[modSlot].ClientViews.TryGetValue(
            invasionId, out BunnyInvasionClientView? view) &&
            view.HasState && (state = view.Current).IsValid();
    }

    private static void SaveInvasionIfChanged(Context context, bool periodicSave = false)
    {
        if (!context.WorldStateInitialized || context.NativeWorldSession <= 0) return;
        foreach (BunnyInvasionSystem system in context.Invasions.Values)
        {
            BunnyInvasionController controller = system.Pipeline.Controller;
            BunnyInvasionSnapshot state = controller.Snapshot();
            if (!context.HudRevisions.TryGetValue(system.Id, out int lastHudRevision) ||
                lastHudRevision != state.Revision)
            {
                if (BunnyInvasionHud.Publish(context.NativeWorldSession,
                    context.Mod, controller))
                    context.HudRevisions[system.Id] = state.Revision;
            }
            if (!context.NetRevisions.TryGetValue(system.Id, out int lastNetRevision) ||
                lastNetRevision != state.Revision)
            {
                if (BunnyInvasionNativeNetwork.Publish(context.NativeWorldSession,
                    context.Mod, controller))
                    context.NetRevisions[system.Id] = state.Revision;
            }
            bool hasSaved = context.SavedInvasions.TryGetValue(system.Id, out var saved);
            int nextPercentStep = state.Progress * 10 / controller.Definition.Goal;
            if (!context.NotifiedInvasions.TryGetValue(system.Id, out var lastNotice) ||
                state.Phase != lastNotice.Phase ||
                nextPercentStep != lastNotice.PercentStep)
            {
                if (BunnyInvasionNotifications.Publish(context.NativeWorldSession,
                    context.Mod, controller))
                    context.NotifiedInvasions[system.Id] = (state.Phase, nextPercentStep);
            }
            if (state.Phase == BunnyInvasionPhase.Completed &&
                (!hasSaved || saved.Phase != BunnyInvasionPhase.Completed) &&
                system.RewardStack > 0 && system.RewardItemClass.Length > 0)
                context.PendingRewards.Add(system.Id);
            if (!hasSaved || state.Phase != saved.Phase ||
                state.Progress != saved.Progress ||
                (periodicSave && state.Cooldown != saved.Cooldown))
            {
                if (BunnyInvasionWorldPersistence.Save(
                    context.NativeWorldSession, context.Mod, controller))
                    context.SavedInvasions[system.Id] =
                        (state.Phase, state.Progress, state.Cooldown);
            }
            if (!context.PendingRewards.Contains(system.Id)) continue;
            int target = context.RewardPlayer >= 0 ?
                context.RewardPlayer : context.LastWorldPlayer;
            if (BunnyInvasionNativeRewards.Submit(context.NativeWorldSession,
                context.Mod, system.Id, state.Epoch, system.RewardItemClass,
                system.RewardStack, target))
                context.PendingRewards.Remove(system.Id);
        }
    }

    private static long Encode(float x, float y)
    {
        if (!float.IsFinite(x) || !float.IsFinite(y) ||
            x < -32f || x > 32f || y < -32f || y > 32f) return -1;
        int dx = Math.Clamp((int)MathF.Round(x * 256f), -8192, 8192);
        int dy = Math.Clamp((int)MathF.Round(y * 256f), -8192, 8192);
        return (1L << 32) | (ushort)(short)dx | ((long)(ushort)(short)dy << 16);
    }

    private static void Fault(Context context, string phase, Exception error)
    {
        context.Faults++;
        context.LastError = phase + ": " + error.GetType().Name + ": " + error.Message;
    }

    public static long TickNpc(int modSlot, int type, int slot, int generation,
        int life, int lifeMax, float x, float y, float vx, float vy, float playerX, float playerY)
        => TickNpc(modSlot, type, slot, generation,
            life, lifeMax, x, y, vx, vy, playerX, playerY, 0L);

    public static long TickNpc(int modSlot, int type, int slot, int generation,
        int life, int lifeMax, float x, float y, float vx, float vy,
        float playerX, float playerY, long nativeIdentity)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 512u || generation < 1 ||
            life <= 0 || !float.IsFinite(x) || !float.IsFinite(y)) return -1;
        Context context = Contexts[modSlot];
        if (!context.Npcs.TryGetValue(type, out BunnyNPC? npc)) return -1;
        ref NpcInstance instance = ref context.NpcSlots[slot];
        bool fresh = instance.Generation != generation || instance.Type != type;
        if (fresh)
        {
            if (instance.Generation > 0 &&
                context.Npcs.TryGetValue(instance.Type, out BunnyNPC? previousNpc))
            {
                var previousState = new BunnyNpcAiState {
                    Slot = slot, Type = instance.Type, Generation = instance.Generation,
                    Age = instance.Age
                };
                try { previousNpc.OnDespawn(ref previousState); }
                catch (Exception error) { Fault(context, previousNpc.GetType().Name + ".OnDespawn", error); }
                foreach (BunnyInvasionSystem invasion in context.Invasions.Values)
                    invasion.Pipeline.Controller.ReportDespawn(true, slot, instance.Generation);
            }
            instance = new NpcInstance { Generation = generation, Type = type };
        }
        if (instance.Blocked) return -1;
        var state = new BunnyNpcAiState {
            Slot = slot, Type = type, Generation = generation, Life = life, LifeMax = lifeMax,
            Age = instance.Age, X = x, Y = y, VelocityX = vx, VelocityY = vy,
            PlayerX = playerX, PlayerY = playerY, NativeIdentity = nativeIdentity
        };
        try
        {
            if (fresh) npc.OnSpawn(ref state);
            foreach (BunnyInvasionSystem invasion in context.Invasions.Values)
                if (invasion.Pipeline.MatchesPendingNpc(npc.GetType().Name, slot, nativeIdentity))
                    invasion.Pipeline.ConfirmFirstTick(true,
                        invasion.Pipeline.Controller.Snapshot().Epoch,
                        npc.GetType().Name, slot, generation, nativeIdentity);
            bool changed = npc.AI(ref state);
            context.NpcCalls++;
            if (instance.Age < int.MaxValue) instance.Age++;
            return changed ? Encode(state.VelocityX, state.VelocityY) : -1;
        }
        catch (Exception error)
        {
            instance.Blocked = true;
            Fault(context, npc.GetType().Name + ".AI", error);
            return -1;
        }
    }

    public static long TickProjectile(int modSlot, int type, int slot, int generation, int owner,
        float x, float y, float vx, float vy, float playerX, float playerY)
        => TickProjectile(modSlot, type, slot, generation, owner,
            x, y, vx, vy, playerX, playerY, -1, 0f, 0f);

    public static long TickProjectile(int modSlot, int type, int slot, int generation, int owner,
        float x, float y, float vx, float vy, float playerX, float playerY,
        int targetNpc, float targetX, float targetY)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 4096u ||
            generation < 1 || !float.IsFinite(x) || !float.IsFinite(y)) return -1;
        Context context = Contexts[modSlot];
        if (!context.Projectiles.TryGetValue(type, out BunnyProjectile? projectile)) return -1;
        ref ProjectileInstance instance = ref context.ProjectileSlots[slot];
        bool fresh = instance.Generation != generation || instance.Type != type ||
                     instance.Owner != owner;
        if (fresh)
        {
            if (instance.Generation > 0 &&
                context.Projectiles.TryGetValue(instance.Type, out BunnyProjectile? oldProjectile))
            {
                var previousState = new BunnyProjectileAiState {
                    Slot = slot, Type = instance.Type, Generation = instance.Generation,
                    Owner = instance.Owner, Age = instance.Age
                };
                try { oldProjectile.OnDespawn(ref previousState); }
                catch (Exception error) { Fault(context, oldProjectile.GetType().Name + ".OnDespawn", error); }
            }
            instance = new ProjectileInstance { Generation = generation, Type = type, Owner = owner };
        }
        if (instance.Blocked) return -1;
        instance.X = x;
        instance.Y = y;
        instance.VelocityX = vx;
        instance.VelocityY = vy;
        var state = new BunnyProjectileAiState {
            Slot = slot, Type = type, Generation = generation, Owner = owner, Age = instance.Age,
            X = x, Y = y, VelocityX = vx, VelocityY = vy, PlayerX = playerX, PlayerY = playerY,
            TargetNpc = targetNpc >= 0 && targetNpc < 200 && float.IsFinite(targetX) &&
                float.IsFinite(targetY) ? targetNpc : -1,
            TargetX = targetX, TargetY = targetY
        };
        try
        {
            if (fresh) projectile.OnSpawn(ref state);
            bool changed = projectile.AI(ref state);
            context.ProjectileCalls++;
            if (instance.Age < int.MaxValue) instance.Age++;
            return changed ? Encode(state.VelocityX, state.VelocityY) : -1;
        }
        catch (Exception error)
        {
            instance.Blocked = true;
            Fault(context, projectile.GetType().Name + ".AI", error);
            return -1;
        }
    }

    public static bool HitNpc(int modSlot, int type, int slot, int generation, int damage, bool killed)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 512u || damage < 0) return false;
        Context context = Contexts[modSlot];
        ref NpcInstance instance = ref context.NpcSlots[slot];
        if (instance.Type != type || instance.Generation != generation || instance.Blocked ||
            !context.Npcs.TryGetValue(type, out BunnyNPC? npc)) return false;
        var state = new BunnyNpcAiState { Slot = slot, Type = type, Generation = generation, Age = instance.Age };
        try
        {
            npc.OnHit(ref state, damage);
            context.NpcHits++;
            if (killed)
            {
                instance.Blocked = true;
                npc.OnKill(ref state);
                foreach (BunnyInvasionSystem invasion in context.Invasions.Values)
                    invasion.Pipeline.Controller.ReportKill(true, slot, generation, npc.GetType().Name);
                context.NpcKills++;
                SaveInvasionIfChanged(context);
            }
            return true;
        }
        catch (Exception error)
        {
            instance.Blocked = true;
            Fault(context, npc.GetType().Name + ".OnHit", error);
            return false;
        }
    }

    public static bool HitProjectile(int modSlot, int type, int slot, int generation, int owner,
        int npcIndex, int damage)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 4096u ||
            npcIndex < 0 || damage < 0) return false;
        Context context = Contexts[modSlot];
        ref ProjectileInstance instance = ref context.ProjectileSlots[slot];
        if (instance.Type != type || instance.Generation != generation ||
            instance.Owner != owner || instance.Blocked ||
            !context.Projectiles.TryGetValue(type, out BunnyProjectile? projectile)) return false;
        var state = new BunnyProjectileAiState {
            Slot = slot, Type = type, Generation = generation, Owner = owner, Age = instance.Age,
            X = instance.X, Y = instance.Y, VelocityX = instance.VelocityX,
            VelocityY = instance.VelocityY
        };
        try
        {
            projectile.OnHitNPC(ref state, npcIndex, damage);
            context.ProjectileHits++;
            return true;
        }
        catch (Exception error)
        {
            instance.Blocked = true;
            Fault(context, projectile.GetType().Name + ".OnHitNPC", error);
            return false;
        }
    }

    public static bool KillNpc(int modSlot, int type, int slot, int generation)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 512u || generation <= 0)
            return false;
        Context context = Contexts[modSlot];
        ref NpcInstance instance = ref context.NpcSlots[slot];
        if (instance.Type != type || instance.Generation != generation || instance.Blocked ||
            !context.Npcs.TryGetValue(type, out BunnyNPC? npc)) return false;
        instance.Blocked = true;
        var state = new BunnyNpcAiState {
            Slot = slot, Type = type, Generation = generation, Age = instance.Age
        };
        try
        {
            npc.OnKill(ref state);
            foreach (BunnyInvasionSystem invasion in context.Invasions.Values)
                invasion.Pipeline.Controller.ReportKill(true, slot, generation, npc.GetType().Name);
            context.NpcKills++;
            SaveInvasionIfChanged(context);
            return true;
        }
        catch (Exception error)
        {
            Fault(context, npc.GetType().Name + ".OnKill", error);
            return false;
        }
    }

    public static bool KillProjectile(int modSlot, int type, int slot, int generation, int owner)
    {
        if ((uint)modSlot >= (uint)Contexts.Count || (uint)slot >= 4096u || generation <= 0)
            return false;
        Context context = Contexts[modSlot];
        ref ProjectileInstance instance = ref context.ProjectileSlots[slot];
        if (instance.Type != type || instance.Generation != generation || instance.Owner != owner ||
            instance.Blocked || !context.Projectiles.TryGetValue(type, out BunnyProjectile? projectile))
            return false;
        instance.Blocked = true;
        var state = new BunnyProjectileAiState {
            Slot = slot, Type = type, Generation = generation, Owner = owner, Age = instance.Age,
            X = instance.X, Y = instance.Y, VelocityX = instance.VelocityX,
            VelocityY = instance.VelocityY
        };
        try
        {
            projectile.OnKill(ref state);
            context.ProjectileKills++;
            return true;
        }
        catch (Exception error)
        {
            Fault(context, projectile.GetType().Name + ".OnKill", error);
            return false;
        }
    }

    public static void ResetWorld()
    {
        foreach (Context context in Contexts)
        {
            for (int i = 0; i < context.NpcSlots.Length; i++)
            {
                NpcInstance current = context.NpcSlots[i];
                if (current.Generation <= 0 || !context.Npcs.TryGetValue(current.Type, out BunnyNPC? npc))
                    continue;
                var state = new BunnyNpcAiState {
                    Slot = i, Type = current.Type, Generation = current.Generation, Age = current.Age
                };
                try { npc.OnDespawn(ref state); }
                catch (Exception error) { Fault(context, npc.GetType().Name + ".ResetWorld", error); }
                foreach (BunnyInvasionSystem invasion in context.Invasions.Values)
                    invasion.Pipeline.Controller.ReportDespawn(true, i, current.Generation);
            }
            for (int i = 0; i < context.ProjectileSlots.Length; i++)
            {
                ProjectileInstance current = context.ProjectileSlots[i];
                if (current.Generation <= 0 ||
                    !context.Projectiles.TryGetValue(current.Type, out BunnyProjectile? projectile))
                    continue;
                var state = new BunnyProjectileAiState {
                    Slot = i, Type = current.Type, Generation = current.Generation,
                    Owner = current.Owner, Age = current.Age
                };
                try { projectile.OnDespawn(ref state); }
                catch (Exception error) { Fault(context, projectile.GetType().Name + ".ResetWorld", error); }
            }
            Array.Clear(context.NpcSlots);
            Array.Clear(context.ProjectileSlots);
        }
    }

    public static string Status(int modSlot)
    {
        if ((uint)modSlot >= (uint)Contexts.Count) return LastRegistrationError;
        Context context = Contexts[modSlot];
        string invasionState = "";
        if (context.InvasionOrder.Count > 0)
        {
            BunnyInvasionSystem invasion = context.InvasionOrder[0];
            BunnyInvasionSnapshot snapshot = invasion.Pipeline.Controller.Snapshot();
            invasionState = " Invasion=" + snapshot.Phase +
                " Progress=" + snapshot.Progress + "/" + invasion.Pipeline.Controller.Definition.Goal +
                " Active=" + snapshot.Active + " Pending=" + snapshot.Pending +
                " Cooldown=" + snapshot.Cooldown +
                " SpawnFailed=" + invasion.Pipeline.FailedCount;
        }
        return "lastUse=" + context.LastUse + invasionState +
            " npc=" + context.NpcCalls + " projectile=" + context.ProjectileCalls +
            " npcHits=" + context.NpcHits + " npcKills=" + context.NpcKills +
            " projectileHits=" + context.ProjectileHits + " projectileKills=" + context.ProjectileKills +
            " faults=" + context.Faults + " last=" + context.LastError;
    }
}
