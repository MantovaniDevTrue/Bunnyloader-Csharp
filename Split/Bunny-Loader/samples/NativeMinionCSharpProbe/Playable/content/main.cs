using System;
using BunnyLoader.Source;

namespace NativeMinionCSharpProbe;

public sealed class NativeMinionProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Minion";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 14;
        Height = 14;
        AiStyle = 0;
        Friendly = true;
        Minion = true;
        MinionSlots = 1f;
        TileCollide = false;
        IgnoreWater = true;
        Penetrate = -1;
        TimeLeft = 18000;
    }

    public override bool AI(ref BunnyProjectileAiState state)
    {
        float dx = state.PlayerX - state.X;
        float dy = state.PlayerY - 40f - state.Y;
        float distanceSq = dx * dx + dy * dy;
        if (distanceSq > 72f * 72f)
        {
            state.VelocityX = Math.Clamp(dx * 0.06f, -9f, 9f);
            state.VelocityY = Math.Clamp(dy * 0.06f, -9f, 9f);
        }
        else
        {
            state.VelocityX *= 0.92f;
            state.VelocityY *= 0.92f;
        }
        return true;
    }
}

public sealed class NativeMinionBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Minion";
        Texture = "Content/Buffs/ExampleDefenseBuff.png";
        Description = "A native C# minion follows the player";
        MinionProjectile = nameof(NativeMinionProjectile);
        NoSave = true;
    }
}

public sealed class NativeMinionStaff : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Summon Staff";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 20;
        UseTime = 20;
        ShootSpeed = 8f;
        Shoot = nameof(NativeMinionProjectile);
        Summon = true;
        MinionBuff = nameof(NativeMinionBuff);
        MinionBuffTime = 2;
    }
}
