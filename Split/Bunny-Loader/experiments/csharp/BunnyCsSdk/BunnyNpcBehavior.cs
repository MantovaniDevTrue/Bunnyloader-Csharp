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
    public virtual void OnDespawn(ref NpcMemory memory) { }
}

public sealed class BunnyNpcRegistry
{
    private const int MaxSlots = 512;
    private readonly Dictionary<string, BunnyNpcBehavior> _names = new(StringComparer.Ordinal);
    private readonly Dictionary<int, BunnyNpcBehavior> _types = new();
    private readonly NpcMemory[] _memory = new NpcMemory[MaxSlots];
    private readonly BunnyNpcBehavior?[] _active = new BunnyNpcBehavior?[MaxSlots];

    public int ActiveInstances { get; private set; }
    public int Spawns { get; private set; }
    public int Releases { get; private set; }

    public void Register(string name, BunnyNpcBehavior behavior)
    {
        _names.Add(name, behavior);
    }

    public bool Bind(int type, string name)
    {
        if (type < 0 || !_names.TryGetValue(name, out var behavior)) return false;
        _types[type] = behavior;
        return true;
    }

    public bool Tick(ref NpcContext context)
    {
        int slot = context.Slot;
        if ((uint)slot >= MaxSlots || context.Generation <= 0 ||
            !_types.TryGetValue(context.Type, out var behavior)) return false;

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
            ActiveInstances++;
            Spawns++;
            behavior.OnSpawn(ref context, ref memory);
        }

        bool changed = behavior.AI(ref context, ref memory);
        if (memory.Age < int.MaxValue) memory.Age++;
        return changed;
    }

    public void Release(int type, int slot, int generation)
    {
        if ((uint)slot >= MaxSlots) return;
        ref NpcMemory state = ref _memory[slot];
        if (_active[slot] != null && state.Type == type && state.Generation == generation)
            ReleaseSlot(slot);
    }

    private void ReleaseSlot(int slot)
    {
        BunnyNpcBehavior? behavior = _active[slot];
        if (behavior == null) return;
        behavior.OnDespawn(ref _memory[slot]);
        _memory[slot] = default;
        _active[slot] = null;
        if (ActiveInstances > 0) ActiveInstances--;
        Releases++;
    }
}
