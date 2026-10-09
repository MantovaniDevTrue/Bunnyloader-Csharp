using System;

namespace SourceCSharpMod;

public static class SourceBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.22f) * 0.03f;
    }
}
