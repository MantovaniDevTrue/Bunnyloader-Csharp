using System;
using System.Collections.Generic;

namespace BunnyLoader.CSharp;

public struct ProjectileMemory
{
    public int Slot;
    public int Type;
    public int Owner;
    public int Generation;
    public int Age;
    public int Counter;
    public float A;
    public float B;
    public float C;
    public float D;
}

public abstract class BunnyProjectileBehavior
{
    public virtual void OnSpawn(ref ProjectileContext context, ref ProjectileMemory memory) { }
    public abstract bool AI(ref ProjectileContext context, ref ProjectileMemory memory);
    public virtual void OnKill(ref ProjectileMemory memory) { }
}

public sealed class BunnyProjectileRegistry
{
    private const int MaxSlots = 4096;

    private readonly Dictionary<string, BunnyProjectileBehavior> _byName =
        new(StringComparer.Ordinal);
    private readonly Dictionary<int, BunnyProjectileBehavior> _byType = new();
    private readonly ProjectileMemory[] _memory = new ProjectileMemory[MaxSlots];
    private readonly BunnyProjectileBehavior?[] _live = new BunnyProjectileBehavior?[MaxSlots];

    public int ActiveInstances { get; private set; }
    public long SpawnCount { get; private set; }
    public long ReleaseCount { get; private set; }

    public void Register(string name, BunnyProjectileBehavior behavior)
    {
        if (string.IsNullOrWhiteSpace(name)) throw new ArgumentException(nameof(name));
        _byName.Add(name, behavior);
    }

    public bool Bind(int type, string name)
    {
        if (type < 0 || !_byName.TryGetValue(name, out var behavior)) return false;
        _byType[type] = behavior;
        return true;
    }

    public bool Tick(ref ProjectileContext context)
    {
        ProjectileView projectile = context.Projectile;
        int slot = projectile.Slot;
        if ((uint)slot >= MaxSlots || projectile.Generation <= 0 ||
            !_byType.TryGetValue(projectile.Type, out var behavior)) return false;

        ref ProjectileMemory memory = ref _memory[slot];
        if (_live[slot] != behavior || memory.Generation != projectile.Generation ||
            memory.Owner != projectile.Owner || memory.Type != projectile.Type)
        {
            ReleaseSlot(slot);
            memory = new ProjectileMemory {
                Slot = slot,
                Type = projectile.Type,
                Owner = projectile.Owner,
                Generation = projectile.Generation
            };
            _live[slot] = behavior;
            ActiveInstances++;
            SpawnCount++;
            behavior.OnSpawn(ref context, ref memory);
        }

        bool changed = behavior.AI(ref context, ref memory);
        if (memory.Age != int.MaxValue) memory.Age++;
        return changed;
    }

    public void Release(int type, int slot, int owner, int generation)
    {
        if ((uint)slot >= MaxSlots) return;
        ref ProjectileMemory memory = ref _memory[slot];
        if (_live[slot] == null || memory.Generation != generation ||
            memory.Type != type || memory.Owner != owner) return;
        ReleaseSlot(slot);
    }

    private void ReleaseSlot(int slot)
    {
        BunnyProjectileBehavior? behavior = _live[slot];
        if (behavior == null) return;
        behavior.OnKill(ref _memory[slot]);
        _memory[slot] = default;
        _live[slot] = null;
        if (ActiveInstances > 0) ActiveInstances--;
        ReleaseCount++;
    }
}
