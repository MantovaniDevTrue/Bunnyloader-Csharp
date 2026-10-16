using System;

namespace BunnyCsPrototype;

public readonly struct PlayerView
{
    public readonly int Index;
    public readonly float X;
    public readonly float Y;

    public PlayerView(int index, float x, float y)
    {
        Index = index;
        X = x;
        Y = y;
    }
}

public readonly struct NpcView
{
    private readonly Exports.NpcSample _sample;

    public NpcView(Exports.NpcSample sample)
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

public readonly struct ProjectileView
{
    private readonly IntPtr _state;

    internal unsafe ProjectileView(Exports.HostProjectileState* state)
    {
        _state = (IntPtr)state;
    }

    private unsafe Exports.HostProjectileState* State => (Exports.HostProjectileState*)_state;

    public unsafe int Slot => State->Slot;
    public unsafe int Type => State->Type;
    public unsafe int Owner => State->Owner;
    public unsafe int Generation => State->Generation;
    public unsafe int TargetIndex => State->TargetIndex;
    public unsafe float X => State->X;
    public unsafe float Y => State->Y;
    public unsafe float VelocityX => State->Vx;
    public unsafe float VelocityY => State->Vy;
    public unsafe float Rotation => State->Rotation;

    public unsafe void SetTargetIndex(int value)
    {
        State->TargetIndex = value;
    }

    public unsafe void SetVelocity(float x, float y)
    {
        State->Vx = x;
        State->Vy = y;
    }

    public unsafe void SetRotation(float value)
    {
        State->Rotation = value;
    }

    public unsafe float Speed
    {
        get
        {
            float x = State->Vx;
            float y = State->Vy;
            return MathF.Sqrt(x * x + y * y);
        }
    }
}

public unsafe struct ProjectileContext
{
    private readonly Exports.NpcSample* _npcs;
    private readonly int _npcCount;

    public ProjectileView Projectile { get; }
    public PlayerView Player { get; }

    internal ProjectileContext(
        Exports.HostProjectileState* state,
        Exports.NpcSample* npcs,
        int npcCount,
        float playerX,
        float playerY)
    {
        Projectile = new ProjectileView(state);
        Player = new PlayerView(state->Owner, playerX, playerY);
        _npcs = npcs;
        _npcCount = npcCount;
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
            Exports.NpcSample sample = _npcs[i];
            NpcView candidate = new(sample);
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

    public bool SteerTowards(float targetX, float targetY, float minimumSpeed, float maximumSpeed, float turn)
    {
        float dx = targetX - Projectile.X;
        float dy = targetY - Projectile.Y;
        float length = MathF.Sqrt(dx * dx + dy * dy);
        if (length < 0.001f) return false;

        float speed = Projectile.Speed;
        if (speed < minimumSpeed) speed = minimumSpeed;
        if (speed > maximumSpeed) speed = maximumSpeed;

        float desiredX = dx / length * speed;
        float desiredY = dy / length * speed;
        float nextX = Projectile.VelocityX + (desiredX - Projectile.VelocityX) * turn;
        float nextY = Projectile.VelocityY + (desiredY - Projectile.VelocityY) * turn;

        float nextLength = MathF.Sqrt(nextX * nextX + nextY * nextY);
        if (nextLength > 0.001f)
        {
            nextX = nextX / nextLength * speed;
            nextY = nextY / nextLength * speed;
        }

        ProjectileView projectile = Projectile;
        projectile.SetVelocity(nextX, nextY);
        projectile.SetRotation(MathF.Atan2(nextY, nextX));
        return true;
    }
}

public interface IProjectileBehavior
{
    bool Update(ref ProjectileContext context);
}

public static class BunnyHost
{
    public const int ApiVersion = 1;

    public static unsafe bool DispatchProjectile(
        Exports.HostProjectileState* state,
        Exports.NpcSample* npcs,
        int npcCount,
        float playerX,
        float playerY)
    {
        return false;
    }
}
