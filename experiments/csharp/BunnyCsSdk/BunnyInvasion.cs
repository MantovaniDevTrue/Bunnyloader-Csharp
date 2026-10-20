using System;
using System.Collections.Generic;
using System.Threading;

namespace BunnyLoader.Source;

public enum BunnyInvasionPhase
{
    Inactive,
    Running,
    Completed
}

public readonly record struct BunnyInvasionEnemy(
    string NpcClass, int Weight, int Points, int UnlockAtProgress = 0);

public sealed class BunnyInvasionDefinition
{
    public string Id { get; }
    public int Goal { get; }
    public int MaxActive { get; }
    public int SpawnInterval { get; }
    public IReadOnlyList<BunnyInvasionEnemy> Enemies { get; }
    internal int TotalWeight { get; }

    public BunnyInvasionDefinition(string id, int goal, int maxActive, int spawnInterval,
        params BunnyInvasionEnemy[] enemies)
    {
        if (string.IsNullOrWhiteSpace(id) || id.Length > 64)
            throw new ArgumentException("Invalid invasion id", nameof(id));
        if (goal < 1 || goal > 1000000)
            throw new ArgumentOutOfRangeException(nameof(goal));
        if (maxActive < 1 || maxActive > 64)
            throw new ArgumentOutOfRangeException(nameof(maxActive));
        if (spawnInterval < 1 || spawnInterval > 36000)
            throw new ArgumentOutOfRangeException(nameof(spawnInterval));
        if (enemies == null || enemies.Length < 1 || enemies.Length > 32)
            throw new ArgumentException("Invasion requires 1..32 enemies", nameof(enemies));

        var names = new HashSet<string>(StringComparer.Ordinal);
        int weight = 0;
        bool hasFirstWave = false;
        foreach (BunnyInvasionEnemy enemy in enemies)
        {
            if (string.IsNullOrWhiteSpace(enemy.NpcClass) ||
                enemy.NpcClass.Length > 128 || !names.Add(enemy.NpcClass) ||
                enemy.Weight < 1 || enemy.Weight > 10000 ||
                enemy.Points < 1 || enemy.Points > goal ||
                enemy.UnlockAtProgress < 0 || enemy.UnlockAtProgress >= goal)
                throw new ArgumentException("Invalid or duplicate invasion enemy", nameof(enemies));
            weight = checked(weight + enemy.Weight);
            hasFirstWave |= enemy.UnlockAtProgress == 0;
        }
        if (!hasFirstWave)
            throw new ArgumentException("An invasion needs an enemy from wave one", nameof(enemies));

        Id = id;
        Goal = goal;
        MaxActive = maxActive;
        SpawnInterval = spawnInterval;
        Enemies = Array.AsReadOnly((BunnyInvasionEnemy[])enemies.Clone());
        TotalWeight = weight;
    }
}

public readonly record struct BunnyInvasionSpawn(int Ticket, string NpcClass)
{
    public int Epoch { get; init; }
}
public readonly record struct BunnyInvasionSnapshot(
    int Epoch, int Revision, BunnyInvasionPhase Phase, int Progress,
    int Active, int Pending, int Cooldown);

public sealed class BunnyInvasionController
{
    private readonly struct NpcKey : IEquatable<NpcKey>
    {
        public readonly int Slot;
        public readonly int Generation;

        public NpcKey(int slot, int generation)
        {
            Slot = slot;
            Generation = generation;
        }

        public bool Equals(NpcKey other) => Slot == other.Slot && Generation == other.Generation;
        public override bool Equals(object? obj) => obj is NpcKey other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(Slot, Generation);
    }

    private readonly Dictionary<NpcKey, BunnyInvasionEnemy> active = new();
    private readonly Dictionary<int, BunnyInvasionEnemy> pending = new();
    private static int globalTicket;
    private int epoch;
    private int revision;
    private int progress;
    private int cooldown;

    public BunnyInvasionDefinition Definition { get; }
    public BunnyInvasionPhase Phase { get; private set; } = BunnyInvasionPhase.Inactive;
    public int Progress => progress;
    public int ActiveCount => active.Count;
    public int PendingCount => pending.Count;
    public bool CanSpawn => Phase == BunnyInvasionPhase.Running &&
        active.Count + pending.Count < Definition.MaxActive && cooldown == 0;
    public bool IsComplete => Phase == BunnyInvasionPhase.Completed;

    public BunnyInvasionController(BunnyInvasionDefinition definition)
    {
        Definition = definition ?? throw new ArgumentNullException(nameof(definition));
    }

    public bool Start(bool authoritative)
    {
        if (!authoritative || Phase == BunnyInvasionPhase.Running) return false;
        active.Clear();
        pending.Clear();
        progress = 0;
        cooldown = 0;
        Phase = BunnyInvasionPhase.Running;
        epoch = NextPositive(epoch);
        revision = NextPositive(revision);
        return true;
    }

    public bool Stop(bool authoritative)
    {
        if (!authoritative || Phase == BunnyInvasionPhase.Inactive) return false;
        Phase = BunnyInvasionPhase.Inactive;
        active.Clear();
        pending.Clear();
        progress = 0;
        cooldown = 0;
        epoch = NextPositive(epoch);
        revision = NextPositive(revision);
        return true;
    }

    public void ResetWorld()
    {
        Phase = BunnyInvasionPhase.Inactive;
        active.Clear();
        pending.Clear();
        progress = 0;
        cooldown = 0;
        epoch = NextPositive(epoch);
        revision = NextPositive(revision);
    }

    public void Tick(bool authoritative, bool playersAvailable)
    {
        if (!authoritative || !playersAvailable || Phase != BunnyInvasionPhase.Running)
            return;
        if (cooldown > 0) cooldown--;
    }

    public bool TryReserveSpawn(bool authoritative, int weightedRoll,
        out BunnyInvasionSpawn spawn)
    {
        spawn = default;
        if (!authoritative || !CanSpawn) return false;
        int unlockedWeight = 0;
        foreach (BunnyInvasionEnemy candidate in Definition.Enemies)
            if (candidate.UnlockAtProgress <= progress)
                unlockedWeight += candidate.Weight;
        if (unlockedWeight <= 0) return false;
        int roll = (int)((uint)weightedRoll % (uint)unlockedWeight);
        BunnyInvasionEnemy selected = default;
        foreach (BunnyInvasionEnemy candidate in Definition.Enemies)
        {
            if (candidate.UnlockAtProgress > progress) continue;
            if (roll < candidate.Weight)
            {
                selected = candidate;
                break;
            }
            roll -= candidate.Weight;
        }

        int ticket = Interlocked.Increment(ref globalTicket) & int.MaxValue;
        if (ticket == 0) ticket = Interlocked.Increment(ref globalTicket) & int.MaxValue;
        while (pending.ContainsKey(ticket) || ticket == 0)
            ticket = Interlocked.Increment(ref globalTicket) & int.MaxValue;
        pending.Add(ticket, selected);
        cooldown = Definition.SpawnInterval;
        revision = NextPositive(revision);
        spawn = new BunnyInvasionSpawn(ticket, selected.NpcClass) { Epoch = epoch };
        return true;
    }

    public bool CommitSpawn(bool authoritative, int spawnTicket,
        string npcClass, int npcSlot, int npcGeneration)
    {
        if (!authoritative || Phase != BunnyInvasionPhase.Running ||
            spawnTicket <= 0 || npcSlot < 0 || npcSlot >= 512 ||
            npcGeneration <= 0 || string.IsNullOrEmpty(npcClass) ||
            !pending.TryGetValue(spawnTicket, out BunnyInvasionEnemy enemy) ||
            enemy.NpcClass != npcClass)
            return false;
        var key = new NpcKey(npcSlot, npcGeneration);
        if (active.ContainsKey(key)) return false;
        if (active.Count >= Definition.MaxActive) return false;
        pending.Remove(spawnTicket);
        active.Add(key, enemy);
        revision = NextPositive(revision);
        return true;
    }

    public bool CancelSpawn(bool authoritative, int spawnTicket)
    {
        if (!authoritative || !pending.Remove(spawnTicket)) return false;
        revision = NextPositive(revision);
        return true;
    }

    public bool ReportKill(bool authoritative, int npcSlot, int npcGeneration,
        string npcClass)
    {
        if (!authoritative || Phase != BunnyInvasionPhase.Running ||
            npcSlot < 0 || npcGeneration <= 0 || string.IsNullOrEmpty(npcClass))
            return false;
        var key = new NpcKey(npcSlot, npcGeneration);
        if (!active.TryGetValue(key, out BunnyInvasionEnemy enemy) ||
            enemy.NpcClass != npcClass)
            return false;
        active.Remove(key);
        progress = (int)Math.Min((long)Definition.Goal, (long)progress + enemy.Points);
        revision = NextPositive(revision);
        if (progress == Definition.Goal)
        {
            Phase = BunnyInvasionPhase.Completed;
            pending.Clear();
            active.Clear();
        }
        return true;
    }

    public bool ReportDespawn(bool authoritative, int npcSlot, int npcGeneration)
    {
        if (!authoritative || !active.Remove(new NpcKey(npcSlot, npcGeneration)))
            return false;
        revision = NextPositive(revision);
        return true;
    }

    public BunnyInvasionSnapshot Snapshot() =>
        new(epoch, revision, Phase, progress, active.Count, pending.Count, cooldown);

    public bool Restore(bool authoritative, BunnyInvasionSnapshot snapshot)
    {
        if (!authoritative || snapshot.Epoch < 0 || snapshot.Revision < 0 ||
            snapshot.Progress < 0 || snapshot.Progress > Definition.Goal ||
            snapshot.Cooldown < 0 || snapshot.Cooldown > Definition.SpawnInterval ||
            !Enum.IsDefined(snapshot.Phase) ||
            (snapshot.Phase == BunnyInvasionPhase.Completed &&
             snapshot.Progress != Definition.Goal) ||
            (snapshot.Phase == BunnyInvasionPhase.Running &&
             snapshot.Progress >= Definition.Goal) ||
            (snapshot.Phase == BunnyInvasionPhase.Inactive &&
             snapshot.Progress != 0))
            return false;

        active.Clear();
        pending.Clear();
        epoch = NextPositive(Math.Max(epoch, snapshot.Epoch));
        revision = NextPositive(Math.Max(revision, snapshot.Revision));
        Phase = snapshot.Phase;
        progress = snapshot.Progress;
        cooldown = snapshot.Phase == BunnyInvasionPhase.Running ? snapshot.Cooldown : 0;
        return true;
    }

    private static int NextPositive(int value) => value == int.MaxValue ? 1 : value + 1;
}
