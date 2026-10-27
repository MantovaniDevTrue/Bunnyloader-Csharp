using BunnyLoader.Source;
namespace ExampleModCSharp;

// Obtenha no catálogo de itens do ambiente de QA. Não consome munição.
public sealed class ExampleCollisionBounceWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example QA Collision Bounce Wand";
        Texture = "Content/Items/Weapons/ExampleCollisionBounceWand.png";
        Damage = 1; UseTime = 30; ShootSpeed = 8;
        Shoot = "ExampleCollisionBounce";
    }
}
