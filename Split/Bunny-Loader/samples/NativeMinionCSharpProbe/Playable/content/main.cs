using System;
using BunnyLoader.Source;

namespace NativeMinionCSharpProbe;

public sealed class NativeMinionProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Minion";
        Texture = "Content/Projectiles/ExampleMinion.png";
        Width = 18;
        Height = 28;
        Frames = 4;
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
        return BunnyMinionSteering.FollowOrAttack(ref state);
    }
}

public sealed class NativeMinionBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Minion";
        Texture = "Content/Buffs/ExampleMinionBuff.png";
        Description = "A native C# minion attacks nearby enemies";
        MinionProjectile = nameof(NativeMinionProjectile);
        NoSave = true;
        NoTimeDisplay = true;
    }
}

public sealed class NativeMinionStaff : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Native C# Summon Staff";
        Texture = "Content/Items/Weapons/Summon/ExampleMinionItem.png";
        Damage = 30;
        UseTime = 36;
        ShootSpeed = 8f;
        Shoot = nameof(NativeMinionProjectile);
        Summon = true;
        MinionBuff = nameof(NativeMinionBuff);
        MinionBuffTime = 2;
    }
}
