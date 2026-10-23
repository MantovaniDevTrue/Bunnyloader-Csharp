using BunnyLoader.Source;

namespace FiveLoadersCSharp;

public sealed class QaWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "QA5 - Varinha de Gore";
        Texture = "Content/Items/Weapons/QaWand.png";
        Damage = (int)QaStats.Damage();
        UseTime = 20;
        ShootSpeed = 8f;
        Shoot = nameof(QaBolt);
    }
    // Madeira 9 é o ID confirmado do Terraria Mobile 301720.
    public override void AddRecipes() => CreateRecipe().AddIngredient(9, 1).Register();
}
