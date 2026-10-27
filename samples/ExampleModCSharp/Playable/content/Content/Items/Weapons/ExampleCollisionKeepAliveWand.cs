using BunnyLoader.Source;
namespace ExampleModCSharp;

// Obtenha no catálogo de itens do ambiente de QA. Não consome munição.
public sealed class ExampleCollisionKeepAliveWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example QA Collision KeepAlive Wand";
        Texture = "Content/Items/Weapons/ExampleCollisionKeepAliveWand.png";
        Damage = 1; UseTime = 30; ShootSpeed = 8;
        Shoot = "ExampleCollisionKeepAlive";
    }
}
