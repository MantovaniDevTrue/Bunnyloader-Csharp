using BunnyLoader.Source;

namespace BunnyGlobalOnlyFixture;

public sealed class GlobalOnlyPolicy : BunnyGlobalItem
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
