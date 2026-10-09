using System;
using BunnyLoader.Source;

namespace SourceCSharpMod;

public sealed class SourceBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Source C# Bolt";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 360;
    }

    public int OnHitBuffId() => 24;

    public override int OnHitNPC(int damage)
    {
        return Math.Clamp(60 + damage * 3, 60, 150);
    }

    public int OnKillBuffId() => 24;

    public override int OnKill(int damage)
    {
        return Math.Clamp(24 + damage * 2, 24, 100);
    }

    public override int OnKillDuration(int damage)
    {
        return Math.Clamp(60 + damage * 2, 60, 160);
    }
}
