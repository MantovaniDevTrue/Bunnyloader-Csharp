using BunnyLoader.Source;

namespace ExampleModCSharp.Rarities;

public sealed class ExampleModRarity : BunnyRarity
{
    public override BunnyColor RarityColor => new(200, 215, 230);

    public override void SetStaticDefaults()
    {
        if (BunnyRarityRuntime.TypeOf<ExampleHigherTierModRarity>("examplemodcsharp") < 12)
            throw new System.InvalidOperationException("Higher-tier rarity must be registered first");
    }

    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset > 0 ? BunnyRarityRuntime.TypeOf<ExampleHigherTierModRarity>("examplemodcsharp") : Type;
}

public sealed class ExampleHigherTierModRarity : BunnyRarity
{
    public override BunnyColor RarityColor
    {
        get
        {
            BunnyColor disco = BunnyRarityRuntime.DiscoColor;
            return new((byte)(disco.R / 2), (byte)(disco.G / 1.25), (byte)(disco.B / 1.5));
        }
    }

    public override int GetPrefixedRarity(int offset, float valueMultiplier) =>
        offset < 0 ? BunnyRarityRuntime.TypeOf<ExampleModRarity>("examplemodcsharp") : Type;
}

public sealed class ExampleRarityItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Item";
        Width = 24;
        Height = 24;
        Rare = 0;
        SetRarity<ExampleModRarity>();
    }
}
