using BunnyLoader.Source;
using System;

namespace SecondExampleModCSharp;

public sealed class SecondBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Second C# Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 12;
        Height = 12;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 1;
        TimeLeft = 220;
    }

    public int OnHitBuffId() => 24;

    public override int OnHitNPC(int damage)
    {
        return Math.Clamp(35 + damage, 35, 120);
    }

    public int OnKillBuffId() => 24;

    public override int OnKill(int damage)
    {
        return Math.Clamp(24 + damage, 24, 96);
    }

    public override int OnKillDuration(int damage)
    {
        return Math.Clamp(35 + damage * 2, 35, 130);
    }
}

public sealed class SecondWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Second C# Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 15;
        UseTime = 17;
        ShootSpeed = 11f;
        Shoot = nameof(SecondBolt);
    }
}
