using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.CSharp;

public static class BunnyApi
{
    public const int NativeModAbi = 1;
    public const int HostApiVersion = 1;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct NpcSample
{
    public int Index;
    public int Type;
    public int Life;
    public int LifeMax;
    public float X;
    public float Y;
    public float Vx;
    public float Vy;
    public int Flags;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct ProjectileState
{
    public int Slot;
    public int Type;
    public int Owner;
    public int Generation;
    public int TargetIndex;
    public float X;
    public float Y;
    public float Vx;
    public float Vy;
    public float Rotation;
}

public readonly struct NpcView
{
    private readonly NpcSample _sample;

    internal NpcView(NpcSample sample)
    {
        _sample = sample;
    }

    public int Index => _sample.Index;
    public int Type => _sample.Type;
    public int Life => _sample.Life;
    public int LifeMax => _sample.LifeMax;
    public float X => _sample.X;
    public float Y => _sample.Y;
    public float VelocityX => _sample.Vx;
    public float VelocityY => _sample.Vy;
    public bool Active => (_sample.Flags & 1) != 0;
    public bool Friendly => (_sample.Flags & 2) != 0;
    public bool DontTakeDamage => (_sample.Flags & 4) != 0;
    public bool IsHostileTarget => Active && !Friendly && !DontTakeDamage && Life > 0;
}

public readonly unsafe struct ProjectileView
{
    private readonly ProjectileState* _state;

    internal ProjectileView(ProjectileState* state)
    {
        _state = state;
    }

    public int Slot => _state->Slot;
    public int Type => _state->Type;
    public int Owner => _state->Owner;
    public int Generation => _state->Generation;
    public int TargetIndex => _state->TargetIndex;
    public float X => _state->X;
    public float Y => _state->Y;
    public float VelocityX => _state->Vx;
    public float VelocityY => _state->Vy;
    public float Rotation => _state->Rotation;

    public float Speed
    {
        get
        {
            float x = _state->Vx;
            float y = _state->Vy;
            return MathF.Sqrt(x * x + y * y);
        }
    }

    public void SetTargetIndex(int value)
    {
        _state->TargetIndex = value;
    }

    public void SetVelocity(float x, float y)
    {
        _state->Vx = x;
        _state->Vy = y;
    }

    public void SetRotation(float value)
    {
        _state->Rotation = value;
    }
}

public unsafe ref struct ProjectileContext
{
    private readonly NpcSample* _npcs;
    private readonly int _npcCount;

    public ProjectileView Projectile { get; }
    public float PlayerX { get; }
    public float PlayerY { get; }

    public ProjectileContext(
        ProjectileState* state,
        NpcSample* npcs,
        int npcCount,
        float playerX,
        float playerY)
    {
        Projectile = new ProjectileView(state);
        PlayerX = playerX;
        PlayerY = playerY;
        _npcs = npcs;
        _npcCount = npcCount < 0 ? 0 : npcCount;
    }

    public int NpcCount => _npcCount;

    public bool TryGetNpc(int index, out NpcView npc)
    {
        if ((uint)index >= (uint)_npcCount)
        {
            npc = default;
            return false;
        }

        npc = new NpcView(_npcs[index]);
        return true;
    }

    public bool TryFindClosestHostile(out NpcView target)
    {
        int best = -1;
        double bestDistance = double.MaxValue;
        float x = Projectile.X;
        float y = Projectile.Y;

        for (int i = 0; i < _npcCount; i++)
        {
            NpcView candidate = new(_npcs[i]);
            if (!candidate.IsHostileTarget) continue;

            double dx = (double)candidate.X - x;
            double dy = (double)candidate.Y - y;
            double distance = dx * dx + dy * dy;
            if (distance >= bestDistance) continue;

            bestDistance = distance;
            best = i;
        }

        if (best < 0)
        {
            target = default;
            return false;
        }

        target = new NpcView(_npcs[best]);
        return true;
    }

    public bool SteerTowards(
        float targetX,
        float targetY,
        float minimumSpeed,
        float maximumSpeed,
        float turn)
    {
        ProjectileView projectile = Projectile;
        float dx = targetX - projectile.X;
        float dy = targetY - projectile.Y;
        float length = MathF.Sqrt(dx * dx + dy * dy);
        if (length < 0.001f) return false;

        float speed = projectile.Speed;
        if (speed < minimumSpeed) speed = minimumSpeed;
        if (speed > maximumSpeed) speed = maximumSpeed;

        float desiredX = dx / length * speed;
        float desiredY = dy / length * speed;
        float nextX = projectile.VelocityX + (desiredX - projectile.VelocityX) * turn;
        float nextY = projectile.VelocityY + (desiredY - projectile.VelocityY) * turn;

        float nextLength = MathF.Sqrt(nextX * nextX + nextY * nextY);
        if (nextLength > 0.001f)
        {
            nextX = nextX / nextLength * speed;
            nextY = nextY / nextLength * speed;
        }

        projectile.SetVelocity(nextX, nextY);
        projectile.SetRotation(MathF.Atan2(nextY, nextX));
        return true;
    }
}

public interface IProjectileBehavior
{
    bool Update(ref ProjectileContext context);
}
