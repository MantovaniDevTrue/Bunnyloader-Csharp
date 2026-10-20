using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleHigherTierModRarity : BunnyRarity
{
    public override BunnyColor RarityColor
    {
        get
        {
            BunnyColor disco = BunnyRarityRuntime.DiscoColor;
            return new((byte)(disco.R / 2), (byte)(disco.G / 1.25),
                (byte)(disco.B / 1.5));
        }
    }

    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset < 0
            ? BunnyRarityRuntime.TypeOf<ExampleModRarity>(ModId)
            : Type;
}
