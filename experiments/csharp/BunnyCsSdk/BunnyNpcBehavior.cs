using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace BunnyLoader.CSharp;

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct NpcState
{
    public int Slot;
    public int Type;
    public int Generation;
    public int Life;
    public int LifeMax;
    public int TargetPlayer;
    public float X;
    public float Y;
    public float VelocityX;
    public float VelocityY;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct NpcCombatCommand
{
    public int Count;
    public int Damage;
    public float X;
    public float Y;
    public float VelocityX0;
    public float VelocityY0;
    public float VelocityX1;
    public float VelocityY1;
}

public unsafe ref struct NpcContext
{
    private readonly NpcState* _state;

    public float PlayerX { get; }
    public float PlayerY { get; }

    public NpcContext(NpcState* state, float playerX, float playerY)
    {
        _state = state;
        PlayerX = playerX;
        PlayerY = playerY;
    }

    public int Slot => _state->Slot;
    public int Type => _state->Type;
    public int Generation => _state->Generation;
    public int Life => _state->Life;
    public int LifeMax => _state->LifeMax;
    public int TargetPlayer => _state->TargetPlayer;
    public float X => _state->X;
    public float Y => _state->Y;
    public float VelocityX => _state->VelocityX;
    public float VelocityY => _state->VelocityY;
    public float LifeRatio => LifeMax > 0 ? Math.Clamp((float)Life / LifeMax, 0f, 1f) : 0f;

    public void SetVelocity(float x, float y)
    {
        _state->VelocityX = x;
        _state->VelocityY = y;
    }

    public bool MoveTowards(float x, float y, float acceleration, float maxSpeed)
    {
        if (!float.IsFinite(x) || !float.IsFinite(y) ||
            !float.IsFinite(acceleration) || !float.IsFinite(maxSpeed) ||
            acceleration <= 0f || maxSpeed <= 0f) return false;

        float dx = x - X;
        float dy = y - Y;
        float distance = MathF.Sqrt(dx * dx + dy * dy);
        if (!float.IsFinite(distance) || distance < 0.001f) return false;

        float ax = dx / distance * maxSpeed - VelocityX;
        float ay = dy / distance * maxSpeed - VelocityY;
        float delta = MathF.Sqrt(ax * ax + ay * ay);
        if (!float.IsFinite(delta)) return false;
        if (delta > acceleration)
        {
            ax *= acceleration / delta;
            ay *= acceleration / delta;
        }

        float vx = VelocityX + ax;
        float vy = VelocityY + ay;
        float speed = MathF.Sqrt(vx * vx + vy * vy);
        if (!float.IsFinite(speed)) return false;
        if (speed > maxSpeed)
        {
            vx *= maxSpeed / speed;
            vy *= maxSpeed / speed;
        }

        SetVelocity(vx, vy);
        return true;
    }
}

public struct NpcMemory
{
    public int Type;
    public int Slot;
    public int Generation;
    public int Age;
    public int Phase;
    public float A;
    public float B;
}

public abstract class BunnyNpcBehavior
{
    public virtual void OnSpawn(ref NpcContext context, ref NpcMemory memory) { }
    public abstract bool AI(ref NpcContext context, ref NpcMemory memory);
    public virtual bool TryAttack(ref NpcContext context, ref NpcMemory memory, out NpcCombatCommand attack)
    {
        attack = default;
        return false;
    }
    public virtual void OnHit(ref NpcMemory memory, int damage) { }
    public virtual void OnKill(ref NpcMemory memory) { }
    public virtual void OnDespawn(ref NpcMemory memory) { }
}

public sealed class BunnyNpcRegistry
{
    private const int MaxSlots = 512;
    private readonly Dictionary<string, BunnyNpcBehavior> _names = new(StringComparer.Ordinal);
    private readonly Dictionary<int, BunnyNpcBehavior> _types = new();
    private readonly NpcMemory[] _memory = new NpcMemory[MaxSlots];
    private readonly BunnyNpcBehavior?[] _active = new BunnyNpcBehavior?[MaxSlots];
    private readonly bool[] _blocked = new bool[MaxSlots];

    public int ActiveInstances { get; private set; }
    public long Spawns { get; private set; }
    public long Releases { get; private set; }
    public long AiCalls { get; private set; }
    public long AttackCalls { get; private set; }
    public long HitCalls { get; private set; }
    public long KillCalls { get; private set; }
    public long Faults { get; private set; }
    public string LastError { get; private set; } = "";

    public void Register(string name, BunnyNpcBehavior behavior)
    {
        if (string.IsNullOrWhiteSpace(name)) throw new ArgumentException(nameof(name));
        ArgumentNullException.ThrowIfNull(behavior);
        _names.Add(name, behavior);
    }

    public bool Bind(int type, string name)
    {
        if (type < 0 || !_names.TryGetValue(name, out var behavior)) return false;
        _types[type] = behavior;
        return true;
    }

    private void Fault(int slot, string phase, Exception error)
    {
        Faults++;
        _blocked[slot] = true;
        LastError = phase + " slot=" + slot + " " + error.GetType().Name + ": " + error.Message;
    }

    private bool Matches(int type, int slot, int generation)
    {
        return (uint)slot < MaxSlots && generation > 0 &&
            _active[slot] != null && !_blocked[slot] &&
            _memory[slot].Type == type && _memory[slot].Generation == generation;
    }

    public bool Tick(ref NpcContext context)
    {
        int slot = context.Slot;
        if ((uint)slot >= MaxSlots || context.Generation <= 0) return false;
        if (!_types.TryGetValue(context.Type, out var behavior))
        {
            ReleaseSlot(slot);
            return false;
        }

        ref NpcMemory memory = ref _memory[slot];
        if (_active[slot] != behavior || memory.Generation != context.Generation ||
            memory.Type != context.Type)
        {
            ReleaseSlot(slot);
            memory = new NpcMemory {
                Slot = slot,
                Type = context.Type,
                Generation = context.Generation
            };
            _active[slot] = behavior;
            _blocked[slot] = false;
            ActiveInstances++;
            Spawns++;
            NpcMemory original = memory;
            float vx = context.VelocityX, vy = context.VelocityY;
            try
            {
                behavior.OnSpawn(ref context, ref memory);
            }
            catch (Exception error)
            {
                memory = original;
                context.SetVelocity(vx, vy);
                Fault(slot, "OnSpawn", error);
                return false;
            }
        }

        if (_blocked[slot]) return false;
        NpcMemory previous = memory;
        float oldX = context.VelocityX, oldY = context.VelocityY;
        try
        {
            bool changed = behavior.AI(ref context, ref memory);
            AiCalls++;
            if (memory.Age < int.MaxValue) memory.Age++;
            return changed;
        }
        catch (Exception error)
        {
            memory = previous;
            context.SetVelocity(oldX, oldY);
            Fault(slot, "AI", error);
            return false;
        }
    }

    public bool TryAttack(ref NpcContext context, out NpcCombatCommand attack)
    {
        attack = default;
        int slot = context.Slot;
        if (!Matches(context.Type, slot, context.Generation) || _memory[slot].Age < 1)
            return false;
        NpcMemory previous = _memory[slot];
        float vx = context.VelocityX, vy = context.VelocityY;
        try
        {
            bool result = _active[slot]!.TryAttack(ref context, ref _memory[slot], out attack);
            AttackCalls++;
            return result;
        }
        catch (Exception error)
        {
            _memory[slot] = previous;
            context.SetVelocity(vx, vy);
            attack = default;
            Fault(slot, "TryAttack", error);
            return false;
        }
    }

    public bool NotifyHit(int type, int slot, int generation, int damage)
    {
        if (!Matches(type, slot, generation) || damage < 0) return false;
        NpcMemory previous = _memory[slot];
        try
        {
            _active[slot]!.OnHit(ref _memory[slot], damage);
            HitCalls++;
            return true;
        }
        catch (Exception error)
        {
            _memory[slot] = previous;
            Fault(slot, "OnHit", error);
            return false;
        }
    }

    public bool NotifyKill(int type, int slot, int generation)
    {
        if (!Matches(type, slot, generation)) return false;
        bool succeeded = true;
        try
        {
            _active[slot]!.OnKill(ref _memory[slot]);
            KillCalls++;
        }
        catch (Exception error)
        {
            Fault(slot, "OnKill", error);
            succeeded = false;
        }
        ReleaseSlot(slot);
        return succeeded;
    }

    public void Release(int type, int slot, int generation)
    {
        if ((uint)slot < MaxSlots && _active[slot] != null &&
            _memory[slot].Type == type && _memory[slot].Generation == generation)
            ReleaseSlot(slot);
    }

    public void ResetWorld()
    {
        for (int i = 0; i < MaxSlots; i++) ReleaseSlot(i);
    }

    private void ReleaseSlot(int slot)
    {
        BunnyNpcBehavior? behavior = _active[slot];
        if (behavior == null) return;
        try
        {
            behavior.OnDespawn(ref _memory[slot]);
        }
        catch (Exception error)
        {
            Fault(slot, "OnDespawn", error);
        }
        finally
        {
            _memory[slot] = default;
            _active[slot] = null;
            _blocked[slot] = false;
            if (ActiveInstances > 0) ActiveInstances--;
            Releases++;
        }
    }
}
