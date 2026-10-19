using BunnyLoader.Source;

namespace BunnyPrefixBridgeFixture;

public sealed class TestBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        Width = 10;
        Height = 10;
        TimeLeft = 300;
    }
}

public sealed class TestWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        Damage = 22;
        Shoot = nameof(TestBolt);
    }
}

public sealed class TestMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 5f;
        DisplayName = "Poderoso";
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Allow(1, 3);
        rules.SetCanRoll(1, false);
        rules.Deny(2);
        rules.SetCanRoll(2, true);
        rules.Allow(512, 7);
        rules.Allow("TestWand", 2);
    }

    public override void SetStats(ref PrefixStats stats)
    {
        stats.DamageMultiplier *= 1.2f;
    }

    public override void ModifyValue(ref float valueMultiplier)
    {
        valueMultiplier *= 1.05f;
    }
}

public sealed class TestCustomPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.Custom;
        RollWeight = 1f;
        DisplayName = "Sob medida";
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Deny(1);
        rules.Allow("TestWand", 0);
        rules.SetCanRoll("TestWand", true);
    }
}

public sealed class TestGuardedPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.Accessory;
        RollWeight = 1f;
    }

    public override void ModifyValue(ref float valueMultiplier)
    {
        valueMultiplier *= 1.2f;
    }

    public override void ApplyAccessoryEffects(ref int defenseBonus)
    {
        defenseBonus += 4;
    }
}
