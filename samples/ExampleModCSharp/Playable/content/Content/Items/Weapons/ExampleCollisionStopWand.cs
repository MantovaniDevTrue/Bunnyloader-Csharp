using BunnyLoader.Source;
namespace ExampleModCSharp;

// Obtenha no catálogo de itens do ambiente de QA. Não consome munição.
public sealed class ExampleCollisionStopWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example QA Collision Stop Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 1; UseTime = 30; ShootSpeed = 8;
        Shoot = "ExampleCollisionStop";
    }
}
