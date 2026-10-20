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

    public override void ModifyValue(ref float valueMultiplier)
    {
        valueMultiplier *= 1.2f;
    }

    public override void ApplyAccessoryEffects(ref int defenseBonus)
    {
        defenseBonus += 4;
    }
}

