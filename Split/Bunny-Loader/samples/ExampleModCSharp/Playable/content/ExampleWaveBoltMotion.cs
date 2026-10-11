using System;

namespace ExampleModCSharp;

public static class ExampleWaveBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.20f) * 0.018f;
    }
}
