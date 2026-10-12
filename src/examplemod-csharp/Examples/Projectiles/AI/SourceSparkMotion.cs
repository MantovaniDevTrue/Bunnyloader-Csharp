using System;

namespace SourceCSharpMod;

public static class SourceSparkMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.32f) * 0.045f;
    }
}
