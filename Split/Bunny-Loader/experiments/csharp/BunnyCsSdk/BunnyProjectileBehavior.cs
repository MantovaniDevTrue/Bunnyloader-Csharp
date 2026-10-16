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
    public virtual void OnHitNPC(ref ProjectileMemory memory, int npcIndex, int damage) { }
    public virtual void OnKill(ref ProjectileMemory memory) { }
}

public sealed class BunnyProjectileRegistry
{
    private const int MaxSlots = 4096;
    private readonly Dictionary<string, BunnyProjectileBehavior> _byName = new(StringComparer.Ordinal);
    private readonly Dictionary<int, BunnyProjectileBehavior> _byType = new();
    private readonly ProjectileMemory[] _memory = new ProjectileMemory[MaxSlots];
    private readonly BunnyProjectileBehavior?[] _live = new BunnyProjectileBehavior?[MaxSlots];
    private readonly bool[] _blocked = new bool[MaxSlots];

    public int ActiveInstances { get; private set; }
    public long SpawnCount { get; private set; }
    public long ReleaseCount { get; private set; }
    public long AiCalls { get; private set; }
    public long HitCalls { get; private set; }
    public long Faults { get; private set; }
    public string LastError { get; private set; } = "";

    public void Register(string name, BunnyProjectileBehavior behavior)
    {
        if (string.IsNullOrWhiteSpace(name)) throw new ArgumentException(nameof(name));
        ArgumentNullException.ThrowIfNull(behavior);
        _byName.Add(name, behavior);
    }

    public bool Bind(int type, string name)
    {
        if (type < 0 || !_byName.TryGetValue(name, out var behavior)) return false;
        _byType[type] = behavior;
        return true;
    }

    private void Fault(int slot, string phase, Exception error)
    {
        Faults++;
        _blocked[slot] = true;
        LastError = phase + " slot=" + slot + " " + error.GetType().Name + ": " + error.Message;
    }

    private bool Matches(int type, int slot, int owner, int generation)
    {
        return (uint)slot < MaxSlots && generation > 0 &&
            _live[slot] != null && !_blocked[slot] &&
            _memory[slot].Generation == generation &&
            _memory[slot].Owner == owner && _memory[slot].Type == type;
    }

    public bool Tick(ref ProjectileContext context)
    {
        ProjectileView projectile = context.Projectile;
        int slot = projectile.Slot;
        if ((uint)slot >= MaxSlots || projectile.Generation <= 0) return false;
        if (!_byType.TryGetValue(projectile.Type, out var behavior))
        {
            ReleaseSlot(slot);
            return false;
        }

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
            _blocked[slot] = false;
            ActiveInstances++;
            SpawnCount++;
            ProjectileMemory previous = memory;
            float vx = projectile.VelocityX, vy = projectile.VelocityY, rotation = projectile.Rotation;
            try
            {
                behavior.OnSpawn(ref context, ref memory);
            }
            catch (Exception error)
            {
                memory = previous;
                projectile.SetVelocity(vx, vy);
                projectile.SetRotation(rotation);
                Fault(slot, "OnSpawn", error);
                return false;
            }
        }

        if (_blocked[slot]) return false;
        ProjectileMemory snapshot = memory;
        float oldX = projectile.VelocityX, oldY = projectile.VelocityY, oldRotation = projectile.Rotation;
        int oldTarget = projectile.TargetIndex;
        try
        {
            bool changed = behavior.AI(ref context, ref memory);
            AiCalls++;
            if (memory.Age < int.MaxValue) memory.Age++;
            return changed;
        }
        catch (Exception error)
        {
            memory = snapshot;
            projectile.SetVelocity(oldX, oldY);
            projectile.SetRotation(oldRotation);
            projectile.SetTargetIndex(oldTarget);
            Fault(slot, "AI", error);
            return false;
        }
    }

    public bool NotifyHitNPC(int type, int slot, int owner, int generation, int npcIndex, int damage)
    {
        if (!Matches(type, slot, owner, generation) || npcIndex < 0 || damage < 0)
            return false;
        ProjectileMemory previous = _memory[slot];
        try
        {
            _live[slot]!.OnHitNPC(ref _memory[slot], npcIndex, damage);
            HitCalls++;
            return true;
        }
        catch (Exception error)
        {
            _memory[slot] = previous;
            Fault(slot, "OnHitNPC", error);
            return false;
        }
    }

    public void Release(int type, int slot, int owner, int generation)
    {
        if ((uint)slot >= MaxSlots || _live[slot] == null ||
            _memory[slot].Generation != generation ||
            _memory[slot].Type != type || _memory[slot].Owner != owner) return;
        ReleaseSlot(slot);
    }

    public void ResetWorld()
    {
        for (int i = 0; i < MaxSlots; i++) ReleaseSlot(i);
    }

    private void ReleaseSlot(int slot)
    {
        BunnyProjectileBehavior? behavior = _live[slot];
        if (behavior == null) return;
        try
        {
            behavior.OnKill(ref _memory[slot]);
        }
        catch (Exception error)
        {
            Fault(slot, "OnKill", error);
        }
        finally
        {
            _memory[slot] = default;
            _live[slot] = null;
            _blocked[slot] = false;
            if (ActiveInstances > 0) ActiveInstances--;
            ReleaseCount++;
        }
    }
}
