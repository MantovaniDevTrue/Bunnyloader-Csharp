// ExampleMod C# | Content/Prefixes/ExampleGuardedPrefix.cs
// Prefixo: o C# prepara os dados; hooks nativos aplicam os modificadores e bonus no Terraria.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGuardedPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Example Protegido";
        Category = PrefixCategory.Accessory;
        RollWeight = 1f;
    }

    public override void ConfigureItemConditions(BunnyPrefixItemConditions conditions)
    {
        conditions.OverrideRollWeightWhenManaBetween(0, 8, 4);
    }

    public override bool CanRoll(BunnyPrefixItemSnapshot item)
        => item.Accessory && item.Stack > 0;

    public override int RollChance(BunnyPrefixItemSnapshot item)
        => item.Mana <= 8 ? 4 : 0;

    public override bool AllStatChangesHaveEffectOn(BunnyPrefixItemSnapshot item)
        => item.Stack > 0;

    public override void Apply(BunnyPrefixItemSnapshot item, ref BunnyPrefixApplyStats effects)
    {
        effects.ValueBonus = item.Mana <= 8 ? 10 : 0;
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

