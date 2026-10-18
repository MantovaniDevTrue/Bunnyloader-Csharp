using System;
using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public sealed class WaveSeekingProjectile : BunnyProjectileBehavior
{
    public override void OnSpawn(ref ProjectileContext context, ref ProjectileMemory memory)
    {
        memory.A = (memory.Slot % 8) * (MathF.PI / 4f);
    }

    public override bool AI(ref ProjectileContext context, ref ProjectileMemory memory)
    {
        if (!context.TryFindClosestHostileInRange(850f, out NpcView target))
        {
            context.Projectile.SetTargetIndex(-1);
            return ProjectileMotion.ApplyDrag(ref context, 0.997f);
        }

        context.Projectile.SetTargetIndex(target.Index);
        float dx = target.X - context.Projectile.X;
        float dy = target.Y - context.Projectile.Y;
        float distance = MathF.Sqrt(dx * dx + dy * dy);
        if (distance < 1f) return false;

        float wave = MathF.Sin(memory.A + memory.Age * 0.12f) * MathF.Min(72f, distance * 0.3f);
        float goalX = target.X - dy / distance * wave;
        float goalY = target.Y + dx / distance * wave;
        return ProjectileMotion.AccelerateTowards(ref context, goalX, goalY, 0.72f, 17f);
    }

    public override void OnKill(ref ProjectileMemory memory)
    {
        memory.A = 0f;
    }
}
