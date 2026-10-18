using System;

namespace BunnyLoader.CSharp.Experimental.Prefix;

public enum PrefixCategoryDraft
{
    Melee = 0,
    Ranged = 1,
    Magic = 2,
    Summon = 3,
    AnyWeapon = 4,
    Accessory = 5,
    Custom = 6
}

public readonly record struct PrefixItemDraft(bool IsWeapon, bool IsAccessory);

public struct PrefixStatsDraft
{
    public float DamageMultiplier { get; set; }
    public float KnockbackMultiplier { get; set; }
    public float UseTimeMultiplier { get; set; }
    public float ScaleMultiplier { get; set; }
    public float ShootSpeedMultiplier { get; set; }
    public float ManaMultiplier { get; set; }
    public int CriticalBonus { get; set; }

    public static PrefixStatsDraft Identity => new()
    {
        DamageMultiplier = 1f,
        KnockbackMultiplier = 1f,
        UseTimeMultiplier = 1f,
        ScaleMultiplier = 1f,
        ShootSpeedMultiplier = 1f,
        ManaMultiplier = 1f
    };
}

public abstract class PrefixDraft
{
    public virtual PrefixCategoryDraft Category => PrefixCategoryDraft.Custom;
    public virtual float RollChance(PrefixItemDraft item) => 1f;
    public virtual bool CanRoll(PrefixItemDraft item) => RollChance(item) > 0f;
    public virtual void SetStats(ref PrefixStatsDraft stats) { }
    public virtual void ModifyValue(ref float multiplier) { }
    public virtual void ApplyAccessoryEffects(ref int defense) { }
}

public class ExampleWeaponPrefixDraft : PrefixDraft
{
    public virtual int Power => 1;
    public override PrefixCategoryDraft Category => PrefixCategoryDraft.AnyWeapon;
    public override float RollChance(PrefixItemDraft item) => 5f;

    public override void SetStats(ref PrefixStatsDraft stats)
    {
        stats.DamageMultiplier *= 1f + 0.20f * Power;
    }

    public override void ModifyValue(ref float multiplier)
    {
        multiplier *= 1f + 0.05f * Power;
    }
}

public sealed class ExampleDerivedPrefixDraft : ExampleWeaponPrefixDraft
{
    public override int Power => base.Power * 2;
}

public sealed class ExampleAccessoryPrefixDraft : PrefixDraft
{
    public override PrefixCategoryDraft Category => PrefixCategoryDraft.Accessory;

    public override void ModifyValue(ref float multiplier)
    {
        multiplier *= 1.2f;
    }

    public override void ApplyAccessoryEffects(ref int defense)
    {
        defense += 4;
    }
}
