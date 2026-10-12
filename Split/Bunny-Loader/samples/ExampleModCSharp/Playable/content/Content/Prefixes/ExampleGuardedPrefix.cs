using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleGuardedPrefix : BunnyPrefix
{
    public override void SetDefaults()
    {
        DisplayName = "Protegido";
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
