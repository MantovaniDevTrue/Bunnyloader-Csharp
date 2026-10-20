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

public sealed class ExampleRarityAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Accessory";
        Tooltip = "Demonstrates the custom rarity on an accessory";
        SetRarity<ExampleHigherTierModRarity>();
    }
}

public sealed class ExampleRarityProjectile : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Projectile";
        Width = 8;
        Height = 8;
        AiStyle = 1;
        TimeLeft = 300;
    }
}

public sealed class ExampleRarityWeapon : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Weapon";
        Damage = 24;
        UseTime = 20;
        Shoot = nameof(ExampleRarityProjectile);
        SetRarity<ExampleModRarity>();
    }
}

public sealed class ExampleRarityBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Buff";
    }
}

public sealed class ExampleRarityPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rarity Potion";
        Buff = nameof(ExampleRarityBuff);
        Duration = 600;
        SetRarity<ExampleHigherTierModRarity>();
    }
}
