using BunnyLoader.Source;

namespace ExampleModCSharp;

[BunnyName("en-US", "Example Sword")]
[BunnyName("pt-BR", "Example Espada")]
public sealed class ExampleMeleeWeapon : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Sword";
        Texture = "Content/Items/Weapons/Melee/ExampleMeleeWeapon.png";
        Melee = true;
        Damage = 50;
        KnockBack = 6;
        Crit = 6;
        UseStyle = 1;
        UseTime = 20;
        UseAnimation = 20;
        AutoReuse = true;
        Value = SellPrice(gold: 1);
        Rare = ItemRarityID.Green;
        UseSound = "Item1";
    }
}
