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

    public override void OnDespawn(ref NpcMemory memory)
    {
        memory = default;
    }
}
