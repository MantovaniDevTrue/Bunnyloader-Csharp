using BunnyLoader.Source;
namespace ExampleModCSharp;

// Exemplo adicional do SDK para testar ExampleBullet sem depender de arma vanilla.
// Não é o port de ExampleGun: aquele exemplo possui áudio personalizado ainda pendente.
public sealed class ExampleBulletLauncher : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Bullet Launcher";
        Texture = "Content/Items/Weapons/Ranged/ExampleBulletLauncher.png";
        Damage = 20; KnockBack = 5; UseTime = 10; UseAnimation = 10;
        UseStyle = 5; AutoReuse = true; NoMelee = true; UseSound = "Item11"; Rare = 2;
        NativeDefaults.SetBool("ranged", true).SetFloat("shootSpeed", 10)
            .SetId("useAmmo", "AmmoID", "Bullet").SetId("shoot", "ProjectileID", "PurificationPowder");
        // O projétil de fallback só entra sem override da munição; ExampleBullet fornece o seu próprio.
        Holdout = new BunnyHoldout(-18, 0);
    }
}
