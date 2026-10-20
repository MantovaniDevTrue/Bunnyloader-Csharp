using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleModRarity : BunnyRarity
{
    public override BunnyColor RarityColor => new(200, 215, 230);

    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset > 0
            ? BunnyRarityRuntime.TypeOf<ExampleHigherTierModRarity>("examplemodcsharp")
            : Type;
}
