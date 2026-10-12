using System;

namespace SourceCSharpMod;

public static class SourceBoltMotion
{
    public static float HomingTurn(float angleError)
    {
        return Math.Clamp(angleError * 0.45f, -0.065f, 0.065f);
    }

    public static int HomingRange() => 480;

    public static int RetargetInterval() => 8;

    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.22f) * 0.03f;
    }
}
