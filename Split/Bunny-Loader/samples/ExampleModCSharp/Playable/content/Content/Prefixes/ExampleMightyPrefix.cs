using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Example Poderoso";
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 5f;
        ExtraTooltip = "Bonus adicional do prefixo";
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Allow("ExampleWand", 3);
    }

    public override void Apply(ref BunnyPrefixApplyStats effects)
    {
        effects.DamageBonus = 3;
        effects.ValueBonus = 40;
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

