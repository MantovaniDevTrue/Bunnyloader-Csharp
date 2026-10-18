using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinionItem : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example Minion Staff";
        Texture = "Content/Items/Weapons/Summon/ExampleMinionItem.png";
        Damage = 30;
        UseTime = 36;
        ShootSpeed = 8f;
        Shoot = nameof(ExampleMinion);
        Summon = true;
        MinionBuff = nameof(ExampleMinionBuff);
        MinionBuffTime = 2;
    }

    public override void AddRecipes()
    {
        CreateRecipe().AddIngredient<ExampleItem>()
            .AddTile<ExampleTiles.Furniture.ExampleWorkbench>().Register();
    }
}
