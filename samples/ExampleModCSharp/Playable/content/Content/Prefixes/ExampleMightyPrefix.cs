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
        AddTooltipLine("Dano extra do prefixo");
        AddTooltipLine("Valor adicional");
        AddTooltipItemLine("Dano atual: {damage}, mana: {mana}, pilha: {stack}");
    }

    public override void ConfigureRollRules(BunnyPrefixRollRules rules)
    {
        rules.Allow("ExampleWand", 3);
    }

    public override void ConfigureItemConditions(BunnyPrefixItemConditions conditions)
    {
        conditions.RequireDamageBetween(10, 25);
        conditions.RequireManaBetween(0, 8);
        conditions.RequireStackBetween(1, 10);
        conditions.OverrideRollWeightWhenStackBetween(3, 5, 6);
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

