using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMightyPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Poderoso";
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
