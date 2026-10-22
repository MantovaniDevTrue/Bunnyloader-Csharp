using BunnyLoader.Source;
namespace ExampleModCSharp;

[BunnyName("en-US", "Example Rocket Launcher")]
[BunnyName("pt-BR", "Example Lança-Foguetes")]
public sealed class ExampleRocketLauncher : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Rocket Launcher";
        Texture = "Content/Items/Weapons/Ranged/ExampleRocketLauncher.png";
        Damage = 55; KnockBack = 4; Crit = 0;
        UseTime = 30; UseAnimation = 30; UseStyle = 5;
        AutoReuse = true; NoMelee = true; Value = BuyPrice(gold: 40); Rare = 8; UseSound = "Item11";
        // A munição escolhe o foguete real e o Terraria conserva sua explosão e colisão.
        NativeDefaults.SetBool("ranged", true).SetFloat("shootSpeed", 5)
            .SetId("shoot", "ProjectileID", "RocketI").SetId("useAmmo", "AmmoID", "Rocket");
        // Referencial local: direção, gravidade e rotação são aplicadas pelo host a cada desenho.
        // Declarar shoot também sincroniza a direção com a mira antes do tiro nativo.
        Holdout = new BunnyHoldout(-2, -2);
    }
}
