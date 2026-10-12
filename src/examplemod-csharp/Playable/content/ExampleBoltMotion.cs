using System;

namespace ExampleModCSharp;

public static class ExampleBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.25f) * 0.025f;
    }
}
