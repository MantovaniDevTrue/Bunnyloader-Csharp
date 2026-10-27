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
        PrefixChance = false;
    }

    public override void ConfigurePrefixRules(BunnyItemPrefixRules rules)
    {
        rules.Deny(71);
        rules.Deny("TestMightyPrefix");
        rules.DenyWhenStackBetween(72, 3, 5);
        rules.DenyWhenStackBetween("TestCustomPrefix", 4, 6);
        rules.DenyWhenDamageBetween(73, 10, 25);
        rules.DenyWhenManaBetween("TestCustomPrefix", 2, 8);
        rules.SetPrefixChanceWhenStackBetween(3, 5, true);
        rules.ChooseWhenStackBetween("TestCustomPrefix", 1, 2);
    }

    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix)
        => prefix != 71 && item.Damage >= 15;

    public override int ChoosePrefix(BunnyPrefixItemSnapshot item)
        => item.Stack == 2 ? 73 : -1;

    public override bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request)
        => request == -1 ? item.Damage >= 20 : (bool?)null;

    public override void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix, ref BunnyPrefixApplyStats effects)
    {
        if (prefix == 72 && item.Damage >= 20)
        {
            effects.DamageBonus = 3;
            effects.ValueBonus = 15;
        }
    }

    public override void ApplyPrefix(ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = 4;
        effects.ValueBonus = 125;
    }
}

public sealed class TestMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 5f;
        DisplayName = "Poderoso";
        ExtraTooltip = "Bonus adicional do prefixo";
        AddTooltipLine("First line");
        AddTooltipLine("Second line");
        AddTooltipItemLine("Dano {damage}, mana {mana}, pilha {stack}");
        RequireEffectiveStats = false;
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

    public override void ConfigureItemConditions(BunnyPrefixItemConditions conditions)
    {
        conditions.RequireDamageBetween(10, 25);
        conditions.RequireManaBetween(0, 8);
        conditions.RequireStackBetween(1, 10);
        conditions.OverrideRollWeightWhenStackBetween(3, 5, 6);
    }

    public override void SetStats(ref PrefixStats stats)
    {
        stats.DamageMultiplier *= 1.2f;
    }

    public override void ModifyValue(ref float valueMultiplier)
    {
        valueMultiplier *= 1.05f;
    }

    public override void Apply(ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = 3;
        effects.ValueBonus = 40;
    }

    public override void Apply(BunnyPrefixItemSnapshot item, ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = item.Damage >= 20 ? 2 : 0;
        effects.ValueBonus = item.Damage >= 20 ? 10 : 0;
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
        rules.PreferFor("TestWand");
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

    public override void ApplyAccessoryEffects(BunnyPrefixPlayerSnapshot player, ref int defenseBonus)
    {
        defenseBonus += player.Life < 200 ? 2 : -1;
    }

    public override void ConfigureItemConditions(BunnyPrefixItemConditions conditions)
    {
        conditions.OverrideRollWeightWhenManaBetween(0, 8, 4);
    }

    public override bool CanRoll(BunnyPrefixItemSnapshot item)
        => item.Damage >= 20 && item.Stack > 0;

    public override int RollChance(BunnyPrefixItemSnapshot item)
        => item.Mana <= 8 ? 4 : 0;

    public override bool AllStatChangesHaveEffectOn(BunnyPrefixItemSnapshot item)
        => item.Damage <= 25;

    public override System.Collections.Generic.IReadOnlyList<BunnyPrefixTooltipLine> GetTooltipLines(
        BunnyPrefixItemSnapshot item)
        => new[] { new BunnyPrefixTooltipLine("Dano " + item.Damage),
                   new BunnyPrefixTooltipLine("Ruim", true) };
}

public sealed class TestGlobal : BunnyGlobalItem
{
    public override bool AppliesTo(BunnyPrefixItemSnapshot item) => item.Damage >= 10;
    public override bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix)
        => prefix != 80;
    public override int ChoosePrefix(BunnyPrefixItemSnapshot item)
        => item.Stack == 3 ? 74 : -1;
    public override bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request)
        => request == -1 ? false : null;
    public override void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix,
                                     ref BunnyPrefixApplyStats effects)
    {
        if (prefix == 73)
        {
            effects.DamageBonus = 2;
            effects.ValueBonus = 5;
        }
    }
}
