// ExampleMod C# | Content/Prefixes/ExampleMightyPrefix.cs
// Prefixo: o C# prepara os dados; hooks nativos aplicam os modificadores e bonus no Terraria.
// Guia do pacote: content/LEIA_PRIMEIRO.md. Receitas adicionais: Common/Recipes/.
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Example Poderoso";
        Category = PrefixCategory.AnyWeapon;
        RollWeight = 5f;
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

