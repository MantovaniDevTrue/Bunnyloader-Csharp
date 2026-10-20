using BunnyLoader.Source;
namespace ExampleModCSharp;

// Porta a espada original: o próprio Terraria calcula a hitbox do balanço,
// aplica o crítico, a repulsão e os prefixos da classe melee.
[BunnyName("en-US", "Example Sword")]
[BunnyName("pt-BR", "Example Espada")]
public sealed class ExampleMeleeWeapon : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Sword";
        Texture = "Content/Items/Weapons/Melee/ExampleMeleeWeapon.png";
        Melee = true; Damage = 50; KnockBack = 6; Crit = 6;
        // Swing conserva o ataque corpo a corpo; não substituímos a espada por um tiro.
        UseStyle = 1; UseTime = 20; UseAnimation = 20; AutoReuse = true;
        Value = SellPrice(gold: 1); Rare = 2; UseSound = "Item1";
        SetRarity<ExampleModRarity>();
    }
}
