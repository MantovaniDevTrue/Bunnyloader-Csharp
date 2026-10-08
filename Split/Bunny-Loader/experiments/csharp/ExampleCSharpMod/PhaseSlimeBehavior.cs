using System;
using BunnyLoader.CSharp;

namespace ExampleCSharpMod;

public sealed class PhaseSlimeBehavior : BunnyNpcBehavior
{
    public override void OnSpawn(ref NpcContext context, ref NpcMemory memory)
    {
        memory.Phase = 1;
        memory.A = (context.Slot % 6) * (MathF.PI / 3f);
    }

    public override bool AI(ref NpcContext context, ref NpcMemory memory)
    {
        int nextPhase = context.LifeRatio > 0.5f ? 1 : 2;
        if (memory.Phase != nextPhase)
        {
            memory.Phase = nextPhase;
            memory.B = memory.Age;
        }

        float speed = memory.Phase == 1 ? 3.4f : 6.2f;
        float acceleration = memory.Phase == 1 ? 0.10f : 0.20f;
        float wave = MathF.Sin(memory.A + memory.Age * (memory.Phase == 1 ? 0.055f : 0.11f));
        float offset = memory.Phase == 1 ? 84f : 128f;

        float dx = context.PlayerX - context.X;
        float dy = context.PlayerY - context.Y;
        float length = MathF.Sqrt(dx * dx + dy * dy);
        if (!float.IsFinite(length) || length < 1f) return false;

        float goalX = context.PlayerX - dy / length * wave * offset;
        float goalY = context.PlayerY + dx / length * wave * offset - 36f;
        return context.MoveTowards(goalX, goalY, acceleration, speed);
    }

    public override bool TryAttack(ref NpcContext context, ref NpcMemory memory, out NpcCombatCommand attack)
    {
        attack = default;
        int period = memory.Phase == 1 ? 140 : 70;
        if (memory.Age < 60 || memory.Age % period != 0) return false;

        float dx = context.PlayerX - context.X;
        float dy = context.PlayerY - context.Y;
        float length = MathF.Sqrt(dx * dx + dy * dy);
        if (!float.IsFinite(length) || length < 1f || length > 1100f) return false;

        float speed = memory.Phase == 1 ? 7.5f : 10f;
        float ux = dx / length;
        float uy = dy / length;
        float spread = memory.Phase == 1 ? 0f : 0.22f;

        attack.Count = memory.Phase == 1 ? 1 : 2;
        attack.Damage = memory.Phase == 1 ? 8 : 12;
        attack.X = context.X;
        attack.Y = context.Y;
        attack.VelocityX0 = (ux * MathF.Cos(spread) - uy * MathF.Sin(spread)) * speed;
        attack.VelocityY0 = (ux * MathF.Sin(spread) + uy * MathF.Cos(spread)) * speed;
        attack.VelocityX1 = (ux * MathF.Cos(-spread) - uy * MathF.Sin(-spread)) * speed;
        attack.VelocityY1 = (ux * MathF.Sin(-spread) + uy * MathF.Cos(-spread)) * speed;
        return true;
    }

    public override void OnDespawn(ref NpcMemory memory)
    {
        memory = default;
    }
}
